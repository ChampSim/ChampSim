#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <getopt.h>
#include <functional>
#include <iomanip>
#include <numeric>
#include <signal.h>
#include <string.h>
#include <vector>

#include "champsim_constants.h"
#include "dram_controller.h"
#include "ooo_cpu.h"
#include "cache.h"
#include "operable.h"
#include "tracereader.h"
#include "vmem.h"

uint8_t MAX_INSTR_DESTINATIONS = NUM_INSTR_DESTINATIONS,
        knob_cloudsuite = 0,
        knob_low_bandwidth = 0;

extern uint8_t show_heartbeat;

uint64_t warmup_instructions     = 1000000,
         simulation_instructions = 10000000;

auto start_time = std::chrono::steady_clock::now();

extern MEMORY_CONTROLLER DRAM;
extern VirtualMemory vmem;
extern std::array<O3_CPU*, NUM_CPUS> ooo_cpu;
extern std::array<CACHE*, NUM_CACHES> caches;
extern std::array<champsim::operable*, NUM_OPERABLES> operables;

std::vector<tracereader*> traces;

std::tuple<uint64_t, uint64_t, uint64_t> elapsed_time()
{
    auto diff = std::chrono::steady_clock::now() - start_time;
    auto elapsed_hour   = std::chrono::duration_cast<std::chrono::hours>(diff);
    auto elapsed_minute = std::chrono::duration_cast<std::chrono::minutes>(diff) - elapsed_hour;
    auto elapsed_second = std::chrono::duration_cast<std::chrono::seconds>(diff) - elapsed_hour - elapsed_minute;
    return {elapsed_hour.count(), elapsed_minute.count(), elapsed_second.count()};
}

void signal_handler(int signal) 
{
	cout << "Caught signal: " << signal << endl;
	exit(1);
}

struct phase_info
{
    std::string name;
    bool is_warmup;
    uint64_t length;
};

int main(int argc, char** argv)
{
	// interrupt signal hanlder
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = signal_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);

    cout << endl << "*** ChampSim Multicore Out-of-Order Simulator ***" << endl << endl;

    show_heartbeat = 1;

    // check to see if knobs changed using getopt_long()
    const struct option long_options[] =
    {
        {"warmup_instructions", required_argument, 0, 'w'},
        {"simulation_instructions", required_argument, 0, 'i'},
        {"hide_heartbeat", no_argument, 0, 'h'},
        {"cloudsuite", no_argument, 0, 'c'},
        {"traces",  no_argument, 0, 't'},
        {0, 0, 0, 0}      
    };

    int option_index = 0; // unused
    bool traces_encountered = false;
    for (int c = getopt_long_only(argc, argv, "wihsb", long_options, &option_index);
            c != -1 && !traces_encountered;
            c = getopt_long_only(argc, argv, "wihsb", long_options, &option_index)
        )
    {
        switch(c) {
            case 'w':
                warmup_instructions = atol(optarg);
                break;
            case 'i':
                simulation_instructions = atol(optarg);
                break;
            case 'h':
                show_heartbeat = 0;
                break;
            case 'c':
                knob_cloudsuite = 1;
                MAX_INSTR_DESTINATIONS = NUM_INSTR_DESTINATIONS_SPARC;
                break;
            case 't':
                traces_encountered = true;
                break;
            default:
                abort();
        }
    }

    // consequences of knobs
    cout << "Warmup Instructions: " << warmup_instructions << endl;
    cout << "Simulation Instructions: " << simulation_instructions << endl;
    cout << "Number of CPUs: " << NUM_CPUS << endl;
    //cout << "LLC sets: " << LLC.NUM_SET << endl;
    //cout << "LLC ways: " << LLC.NUM_WAY << endl;
    std::cout << "Off-chip DRAM Size: " << (DRAM_CHANNELS*DRAM_RANKS*DRAM_BANKS*DRAM_ROWS*DRAM_ROW_SIZE/1024) << " MB Channels: " << DRAM_CHANNELS << " Width: " << 8*DRAM_CHANNEL_WIDTH << "-bit Data Rate: " << DRAM_IO_FREQ << " MT/s" << std::endl;

    std::cout << std::endl;
    std::cout << "VirtualMemory physical capacity: " << std::size(vmem.ppage_free_list) * vmem.page_size;
    std::cout << " num_ppages: " << std::size(vmem.ppage_free_list) << std::endl;
    std::cout << "VirtualMemory page size: " << PAGE_SIZE << " log2_page_size: " << LOG2_PAGE_SIZE << std::endl;

    // end consequence of knobs

    // search through the argv for "-traces"
    std::cout << std::endl;
    for (auto it = std::next(std::find(argv, std::next(argv, argc), std::string{"-traces"})); it != std::next(argv, argc); it++)
    {
        std::cout << "CPU " << traces.size() << " runs " << *it << std::endl;
        traces.push_back(get_tracereader(*it, traces.size(), knob_cloudsuite));
    }

    if (traces.size() != NUM_CPUS)
    {
        std::cout << std::endl << "*** Number of traces does not match number of cores ***" << std::endl << std::endl;
        abort();
    }
    // end trace file setup

    for (O3_CPU* cpu : ooo_cpu)
        cpu->initialize_core();

    for (auto it = caches.rbegin(); it != caches.rend(); ++it)
    {
        (*it)->impl_prefetcher_initialize();
        (*it)->impl_replacement_initialize();
    }

    std::vector<phase_info> phases{{
        phase_info{"Warmup", true, warmup_instructions},
        phase_info{"Simulation", false, simulation_instructions}
    }};

    // simulation entry point
    for (auto phase : phases)
    {
        // Initialize phase
        for (auto op : operables)
        {
            op->warmup = phase.is_warmup;
            op->begin_phase();
        }

        // Perform phase
        std::bitset<NUM_CPUS> phase_complete = {};
        while (!phase_complete.all())
        {
            // Operate
            for (auto op : operables)
            {
                try
                {
                    op->_operate();
                }
                catch (champsim::deadlock &dl)
                {
                    //ooo_cpu[dl.which]->print_deadlock();
                    //std::cout << std::endl;
                    //for (auto c : caches)
                    for (auto c : operables)
                    {
                        c->print_deadlock();
                        std::cout << std::endl;
                    }

                    abort();
                }
            }
            std::sort(std::begin(operables), std::end(operables), champsim::by_next_operate());

            // Read from trace
            for (auto cpu : ooo_cpu)
            {
                while (cpu->fetch_stall == 0 && cpu->instrs_to_read_this_cycle > 0)
                    cpu->init_instruction(traces[cpu->cpu]->get()); // read from trace
            }

            // Check for phase finish
            auto [elapsed_hour, elapsed_minute, elapsed_second] = elapsed_time();
            for (auto cpu : ooo_cpu)
            {
                // Phase complete
                if (!phase_complete[cpu->cpu] && (cpu->sim_instr() >= phase.length))
                {
                    phase_complete.set(cpu->cpu);
                    for (auto op : operables)
                        op->end_phase(cpu->cpu);

                    std::cout << phase.name << " finished CPU " << cpu->cpu;
                    std::cout << " instructions: " << cpu->sim_instr() << " cycles: " << cpu->sim_cycle() << " cumulative IPC: " << 1.0 * cpu->sim_instr() / cpu->sim_cycle();
                    std::cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << std::endl;
                }
            }
        }

        auto [elapsed_hour, elapsed_minute, elapsed_second] = elapsed_time();
        for (auto cpu : ooo_cpu)
        {
            std::cout << std::endl;
            std::cout << phase.name << " complete CPU " << cpu->cpu << " instructions: " << cpu->sim_instr() << " cycles: " << cpu->sim_cycle();
            std::cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << std::endl;
            std::cout << std::endl;
        }
    }

    std::cout << "ChampSim completed all CPUs" << std::endl;

    if (NUM_CPUS > 1)
    {
        std::cout << std::endl;
        std::cout << "Total Simulation Statistics (not including warmup)" << std::endl;
        for (auto cpu : ooo_cpu)
        {
            std::cout << std::endl;
            std::cout << "CPU " << cpu->cpu << " cumulative IPC: " << 1.0 * cpu->sim_instr() / cpu->sim_cycle();
            std::cout << " instructions: " << cpu->sim_instr() << " cycles: " << cpu->sim_cycle();
            std::cout << std::endl;

            for (auto it = caches.rbegin(); it != caches.rend(); ++it)
                (*it)->print_phase_stats();
        }
    }

    std::cout << std::endl;
    std::cout << "Region of Interest Statistics" << std::endl;
    for (auto cpu : ooo_cpu)
    {
        std::cout << std::endl;
        std::cout << "CPU " << cpu->cpu << " cumulative IPC: " << (1.0 * cpu->roi_instr() / cpu->roi_cycle());
        std::cout << " instructions: " << cpu->roi_cycle() << " cycles: " << cpu->roi_cycle();
        std::cout << std::endl;

        for (auto it = caches.rbegin(); it != caches.rend(); ++it)
            (*it)->print_roi_stats();
    }

    for (auto cpu : ooo_cpu)
        cpu->print_phase_stats();

    for (auto it = caches.rbegin(); it != caches.rend(); ++it)
        (*it)->impl_prefetcher_final_stats();

    for (auto it = caches.rbegin(); it != caches.rend(); ++it)
        (*it)->impl_replacement_final_stats();

    DRAM.print_phase_stats();

    return 0;
}

