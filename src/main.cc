#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <getopt.h>
#include <functional>
#include <iomanip>
#include <signal.h>
#include <string.h>
#include <vector>

#include "champsim_constants.h"
#include "dram_controller.h"
#include "ooo_cpu.h"
#include "operable.h"
#include "tracereader.h"

uint8_t warmup_complete[NUM_CPUS] = {},
        MAX_INSTR_DESTINATIONS = NUM_INSTR_DESTINATIONS,
        knob_cloudsuite = 0,
        knob_low_bandwidth = 0;

extern uint8_t show_heartbeat;

uint64_t warmup_instructions     = 1000000,
         simulation_instructions = 10000000;

auto start_time = std::chrono::steady_clock::now();

extern CACHE LLC;
extern MEMORY_CONTROLLER DRAM;
extern std::array<O3_CPU*, NUM_CPUS> ooo_cpu;
extern std::array<champsim::operable*, 7*NUM_CPUS+2> operables;

std::vector<tracereader*> traces;

std::tuple<uint64_t, uint64_t, uint64_t> elapsed_time()
{
    auto diff = std::chrono::steady_clock::now() - start_time;
    auto elapsed_hour   = std::chrono::duration_cast<std::chrono::hours>(diff);
    auto elapsed_minute = std::chrono::duration_cast<std::chrono::minutes>(diff) - elapsed_hour;
    auto elapsed_second = std::chrono::duration_cast<std::chrono::seconds>(diff) - elapsed_hour - elapsed_minute;
    return {elapsed_hour.count(), elapsed_minute.count(), elapsed_second.count()};
}

void print_deadlock(uint32_t i)
{
    cout << "DEADLOCK! CPU " << i << " instr_id: " << ooo_cpu[i]->ROB.front().instr_id;
    cout << " translated: " << +ooo_cpu[i]->ROB.front().translated;
    cout << " fetched: " << +ooo_cpu[i]->ROB.front().fetched;
    cout << " scheduled: " << +ooo_cpu[i]->ROB.front().scheduled;
    cout << " executed: " << +ooo_cpu[i]->ROB.front().executed;
    cout << " is_memory: " << +ooo_cpu[i]->ROB.front().is_memory;
    cout << " num_reg_dependent: " << +ooo_cpu[i]->ROB.front().num_reg_dependent;
    cout << " event: " << ooo_cpu[i]->ROB.front().event_cycle;
    cout << " current: " << ooo_cpu[i]->current_cycle << endl;

    // print LQ entry
    std::cout << std::endl << "Load Queue Entry" << std::endl;
    for (auto lq_it = std::begin(ooo_cpu[i]->LQ); lq_it != std::end(ooo_cpu[i]->LQ); ++lq_it)
    {
        std::cout << "[LQ] entry: " << std::distance(std::begin(ooo_cpu[i]->LQ), lq_it) << " instr_id: " << lq_it->instr_id << " address: " << std::hex << lq_it->physical_address << std::dec << " translated: " << +lq_it->translated << " fetched: " << +lq_it->fetched << std::endl;
    }

    // print SQ entry
    std::cout << std::endl << "Store Queue Entry" << std::endl;
    for (auto sq_it = std::begin(ooo_cpu[i]->SQ); sq_it != std::end(ooo_cpu[i]->SQ); ++sq_it)
    {
        std::cout << "[SQ] entry: " << std::distance(std::begin(ooo_cpu[i]->SQ), sq_it) << " instr_id: " << sq_it->instr_id << " address: " << std::hex << sq_it->physical_address << std::dec << " translated: " << +sq_it->translated << " fetched: " << +sq_it->fetched << std::endl;
    }

    // print L1D MSHR entry
    std::cout << std::endl << "L1D MSHR Entry" << std::endl;
    std::size_t j = 0;
    for (PACKET &entry : static_cast<CACHE*>(ooo_cpu[i]->L1D_bus.lower_level)->MSHR) {
        std::cout << "[L1D MSHR] entry: " << j << " instr_id: " << entry.instr_id;
        std::cout << " address: " << std::hex << entry.address << " full_addr: " << entry.full_addr << std::dec << " type: " << +entry.type;
        std::cout << " fill_level: " << entry.fill_level << " event_cycle: " << entry.event_cycle << std::endl;
        ++j;
    }

    assert(0);
}

void signal_handler(int signal) 
{
	cout << "Caught signal: " << signal << endl;
	exit(1);
}

void cpu_l1i_prefetcher_cache_operate(uint32_t cpu_num, uint64_t v_addr, uint8_t cache_hit, uint8_t prefetch_hit)
{
  ooo_cpu[cpu_num]->l1i_prefetcher_cache_operate(v_addr, cache_hit, prefetch_hit);
}

void cpu_l1i_prefetcher_cache_fill(uint32_t cpu_num, uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr)
{
  ooo_cpu[cpu_num]->l1i_prefetcher_cache_fill(addr, set, way, prefetch, evicted_addr);
}

struct phase_info
{
    std::string name;
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
    int c;
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
    cout << "LLC sets: " << LLC.NUM_SET << endl;
    cout << "LLC ways: " << LLC.NUM_WAY << endl;
    std::cout << "Off-chip DRAM Size: " << (DRAM_CHANNELS*DRAM_RANKS*DRAM_BANKS*DRAM_ROWS*DRAM_ROW_SIZE/1024) << " MB Channels: " << DRAM_CHANNELS << " Width: " << 8*DRAM_CHANNEL_WIDTH << "-bit Data Rate: " << DRAM_IO_FREQ << " MT/s" << std::endl;

    // end consequence of knobs

    // search through the argv for "-traces"
    std::cout << std::endl;
    for (auto it = std::find(argv, std::next(argv, argc), std::string{"-traces"}); it != std::next(argv, argc); it++)
    {
        std::cout << "CPU " << traces.size() << " runs " << it << std::endl;
        traces.push_back(get_tracereader(it, traces.size(), knob_cloudsuite));
    }

    if (traces.size() != NUM_CPUS)
    {
        std::cout << std::endl << "*** Number of traces does not match number of cores ***" << std::endl << std::endl;
        abort();
    }
    // end trace file setup

    for (auto cpu : ooo_cpu) {
        static_cast<CACHE*>(cpu->L1I_bus.lower_level)->l1i_prefetcher_cache_operate = cpu_l1i_prefetcher_cache_operate;
        static_cast<CACHE*>(cpu->L1I_bus.lower_level)->l1i_prefetcher_cache_fill = cpu_l1i_prefetcher_cache_fill;
    }

    // SHARED CACHE
    LLC.cache_type = IS_LLC;
    LLC.fill_level = FILL_LLC;

    using namespace std::placeholders;
    LLC.find_victim = std::bind(&CACHE::llc_find_victim, &LLC, _1, _2, _3, _4, _5, _6, _7);
    LLC.update_replacement_state = std::bind(&CACHE::llc_update_replacement_state, &LLC, _1, _2, _3, _4, _5, _6, _7, _8);
    LLC.replacement_final_stats = std::bind(&CACHE::lru_final_stats, &LLC);

    LLC.llc_initialize_replacement();
    LLC.llc_prefetcher_initialize();

    std::vector<phase_info> phases{{
        phase_info{"Warmup", warmup_instructions},
        phase_info{"Simulation", simulation_instructions}
    }};

    // simulation entry point
    for (auto phase : phases)
    {
        // Initialize phase
        for (auto op : operables)
            op->begin_phase();

        // Perform phase
        std::bitset<NUM_CPUS> phase_complete = {};
        while (!phase_complete.all())
        {
            // Operate
            for (auto op : operables)
                op->_operate();
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
                // Keep warmup_complete
                warmup_complete[cpu->cpu] = (cpu->num_retired >= warmup_instructions);

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

    if (NUM_CPUS > 1) {
        std::cout << std::endl;
        std::cout << "Total Simulation Statistics (not including warmup)" << std::endl;
        for (auto cpu : ooo_cpu)
        {
            std::cout << std::endl;
            std::cout << "CPU " << cpu->cpu << " cumulative IPC: " << 1.0 * cpu->sim_instr() / cpu->sim_cycle();
            std::cout << " instructions: " << cpu->sim_instr() << " cycles: " << cpu->sim_cycle() << endl;

            static_cast<CACHE*>(cpu->L1D_bus.lower_level)->print_phase_stats();
            static_cast<CACHE*>(cpu->L1I_bus.lower_level)->print_phase_stats();
            static_cast<CACHE*>(static_cast<CACHE*>(cpu->L1D_bus.lower_level)->lower_level)->print_phase_stats();

            cpu->l1i_prefetcher_final_stats();
            static_cast<CACHE*>(cpu->L1D_bus.lower_level)->l1d_prefetcher_final_stats();
            static_cast<CACHE*>(static_cast<CACHE*>(cpu->L1D_bus.lower_level)->lower_level)->l2c_prefetcher_final_stats();
        }

        LLC.print_phase_stats();
        LLC.llc_prefetcher_final_stats();
    }

    std::cout << std::endl;
    std::cout << "Region of Interest Statistics" << std::endl;
    for (auto cpu : ooo_cpu)
    {
        std::cout << std::endl;
        std::cout << "CPU " << cpu->cpu << " cumulative IPC: " << (1.0 * cpu->roi_instr() / cpu->roi_cycle());
        std::cout << " instructions: " << cpu->roi_cycle() << " cycles: " << cpu->roi_cycle();
        std::cout << endl;

        static_cast<CACHE*>(cpu->L1D_bus.lower_level)->print_roi_stats();
        static_cast<CACHE*>(cpu->L1I_bus.lower_level)->print_roi_stats();
        static_cast<CACHE*>(static_cast<CACHE*>(cpu->L1D_bus.lower_level)->lower_level)->print_roi_stats();

        static_cast<CACHE*>(cpu->DTLB_bus.lower_level)->print_roi_stats();
        static_cast<CACHE*>(cpu->ITLB_bus.lower_level)->print_roi_stats();
        static_cast<CACHE*>(static_cast<CACHE*>(cpu->DTLB_bus.lower_level)->lower_level)->print_roi_stats();
    }

    LLC.print_roi_stats();

    for (auto cpu : ooo_cpu)
    {
        cpu->print_phase_stats();
        cpu->l1i_prefetcher_final_stats();
        static_cast<CACHE*>(cpu->L1D_bus.lower_level)->l1d_prefetcher_final_stats();
        static_cast<CACHE*>(static_cast<CACHE*>(cpu->L1D_bus.lower_level)->lower_level)->l2c_prefetcher_final_stats();
    }

    LLC.llc_prefetcher_final_stats();
    LLC.llc_replacement_final_stats();
    DRAM.print_phase_stats();

    return 0;
}
