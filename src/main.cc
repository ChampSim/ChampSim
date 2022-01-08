#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <getopt.h>
#include <fstream>
#include <functional>
#include <iomanip>
#include <numeric>
#include <signal.h>
#include <string.h>
#include <vector>

#include "champsim_constants.h"
#include "dram_controller.h"
#include "ooo_cpu.h"
#include "operable.h"
#include "vmem.h"
#include "tracereader.h"

uint8_t MAX_INSTR_DESTINATIONS = NUM_INSTR_DESTINATIONS,
        knob_cloudsuite = 0,
        knob_low_bandwidth = 0;

extern uint8_t show_heartbeat;

uint64_t warmup_instructions     = 1000000,
         simulation_instructions = 10000000,
         champsim_seed;

auto start_time = std::chrono::steady_clock::now();

extern CACHE LLC;
extern MEMORY_CONTROLLER DRAM;
extern VirtualMemory vmem;
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

void print_cache_stats(std::string name, uint32_t cpu, CACHE::stats_type stats)
{
    uint64_t TOTAL_HIT = std::accumulate(std::begin(stats.hits.at(cpu)), std::end(stats.hits[cpu]), 0ull),
             TOTAL_MISS = std::accumulate(std::begin(stats.hits.at(cpu)), std::end(stats.hits[cpu]), 0ull);

    std::cout << name << " TOTAL     ";
    std::cout << "ACCESS: " << std::setw(10) << TOTAL_HIT + TOTAL_MISS << "  ";
    std::cout << "HIT: "    << std::setw(10) << TOTAL_HIT << "  ";
    std::cout << "MISS: "   << std::setw(10) << TOTAL_MISS << std::endl;

    std::cout << name << " LOAD      ";
    std::cout << "ACCESS: " << std::setw(10) << stats.hits[cpu][LOAD] + stats.misses[cpu][LOAD] << "  ";
    std::cout << "HIT: "    << std::setw(10) << stats.hits[cpu][LOAD] << "  ";
    std::cout << "MISS: "   << std::setw(10) << stats.misses[cpu][LOAD] << std::endl;

    std::cout << name << " RFO       ";
    std::cout << "ACCESS: " << std::setw(10) << stats.hits[cpu][RFO] + stats.misses[cpu][RFO] << "  ";
    std::cout << "HIT: "    << std::setw(10) << stats.hits[cpu][RFO] << "  ";
    std::cout << "MISS: "   << std::setw(10) << stats.misses[cpu][RFO] << std::endl;

    std::cout << name << " PREFETCH  ";
    std::cout << "ACCESS: " << std::setw(10) << stats.hits[cpu][PREFETCH] + stats.misses[cpu][PREFETCH] << "  ";
    std::cout << "HIT: "    << std::setw(10) << stats.hits[cpu][PREFETCH] << "  ";
    std::cout << "MISS: "   << std::setw(10) << stats.misses[cpu][PREFETCH] << std::endl;

    std::cout << name << " WRITEBACK ";
    std::cout << "ACCESS: " << std::setw(10) << stats.hits[cpu][WRITEBACK] + stats.misses[cpu][WRITEBACK] << "  ";
    std::cout << "HIT: "    << std::setw(10) << stats.hits[cpu][WRITEBACK] << "  ";
    std::cout << "MISS: "   << std::setw(10) << stats.misses[cpu][WRITEBACK] << std::endl;

    std::cout << name << " TRANSLATION ";
    std::cout << "ACCESS: " << std::setw(10) << stats.hits[cpu][TRANSLATION] + stats.misses[cpu][TRANSLATION] << "  ";
    std::cout << "HIT: "    << std::setw(10) << stats.hits[cpu][TRANSLATION] << "  ";
    std::cout << "MISS: "   << std::setw(10) << stats.misses[cpu][TRANSLATION] << std::endl;

    std::cout << name << " PREFETCH  ";
    std::cout << "REQUESTED: " << std::setw(10) << stats.pf_requested << "  ";
    std::cout << "ISSUED: " << std::setw(10) << stats.pf_issued << "  ";
    std::cout << "USEFUL: " << std::setw(10) << stats.pf_useful << "  ";
    std::cout << "USELESS: " << std::setw(10) << stats.pf_useless << std::endl;

    std::cout << name << " AVERAGE MISS LATENCY: " << (1.0*(stats.total_miss_latency))/TOTAL_MISS << " cycles" << std::endl;
    //std::cout << " AVERAGE MISS LATENCY: " << (stats.total_miss_latency)/TOTAL_MISS << " cycles " << stats.total_miss_latency << "/" << TOTAL_MISS<< std::endl;
}

void print_cpu_stats(uint32_t index, O3_CPU::stats_type stats, uint64_t begin_instr, uint64_t end_instr)
{
    auto total_branch         = std::accumulate(std::begin(stats.total_branch_types), std::end(stats.total_branch_types), 0ull);
    auto total_mispredictions = std::accumulate(std::begin(stats.branch_type_misses), std::end(stats.branch_type_misses), 0ull);

    std::cout << "CPU " << index << " Branch Prediction Accuracy: ";
    std::cout << (100.0*(total_branch - total_mispredictions)) / total_branch;
    std::cout << "% MPKI: " << (1000.0*total_mispredictions)/(end_instr - begin_instr);
    std::cout << " Average ROB Occupancy at Mispredict: " << (1.0*stats.total_rob_occupancy_at_branch_mispredict)/total_mispredictions << std::endl;

    std::cout << "Branch type MPKI" << std::endl;
    std::cout << "BRANCH_DIRECT_JUMP: "   << (1000.0*stats.branch_type_misses[BRANCH_DIRECT_JUMP]  /(end_instr - begin_instr)) << " (" << stats.branch_type_misses[BRANCH_DIRECT_JUMP] << "/" << stats.total_branch_types[BRANCH_DIRECT_JUMP]   << ")" << std::endl;
    std::cout << "BRANCH_INDIRECT: "      << (1000.0*stats.branch_type_misses[BRANCH_INDIRECT]     /(end_instr - begin_instr)) << " (" << stats.branch_type_misses[BRANCH_INDIRECT] << "/" << stats.total_branch_types[BRANCH_INDIRECT]      << ")" << std::endl;
    std::cout << "BRANCH_CONDITIONAL: "   << (1000.0*stats.branch_type_misses[BRANCH_CONDITIONAL]  /(end_instr - begin_instr)) << " (" << stats.branch_type_misses[BRANCH_CONDITIONAL] << "/" << stats.total_branch_types[BRANCH_CONDITIONAL]   << ")" << std::endl;
    std::cout << "BRANCH_DIRECT_CALL: "   << (1000.0*stats.branch_type_misses[BRANCH_DIRECT_CALL]  /(end_instr - begin_instr)) << " (" << stats.branch_type_misses[BRANCH_DIRECT_CALL] << "/" << stats.total_branch_types[BRANCH_DIRECT_CALL]   << ")" << std::endl;
    std::cout << "BRANCH_INDIRECT_CALL: " << (1000.0*stats.branch_type_misses[BRANCH_INDIRECT_CALL]/(end_instr - begin_instr)) << " (" << stats.branch_type_misses[BRANCH_INDIRECT_CALL] << "/" << stats.total_branch_types[BRANCH_INDIRECT_CALL] << ")" << std::endl;
    std::cout << "BRANCH_RETURN: "        << (1000.0*stats.branch_type_misses[BRANCH_RETURN]       /(end_instr - begin_instr)) << " (" << stats.branch_type_misses[BRANCH_RETURN] << "/" << stats.total_branch_types[BRANCH_RETURN]        << ")" << std::endl;
    std::cout << std::endl;
}

void print_dram_channel_stats(uint32_t index, DRAM_CHANNEL::stats_type stats)
{
    std::cout << " CHANNEL " << index << std::endl;
    std::cout << " RQ ROW_BUFFER_HIT: " << std::setw(10) << stats.RQ_ROW_BUFFER_HIT << "  ROW_BUFFER_MISS: " << std::setw(10) << stats.RQ_ROW_BUFFER_MISS << std::endl;
    std::cout << " AVG DBUS CONGESTED CYCLE: ";
    if (stats.dbus_count_congested > 0)
        std::cout << std::setw(10) << (1.0*stats.dbus_cycle_congested) / stats.dbus_count_congested;
    else
        std::cout << "-";
    std::cout << std::endl;
    std::cout << " WQ ROW_BUFFER_HIT: " << std::setw(10) << stats.WQ_ROW_BUFFER_HIT << "  ROW_BUFFER_MISS: " << std::setw(10) << stats.WQ_ROW_BUFFER_MISS;
    std::cout << "  FULL: " << std::setw(10) << stats.WQ_FULL << std::endl;
    std::cout << std::endl;
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

    uint32_t seed_number = 0;

    // check to see if knobs changed using getopt_long()
    int c;
    while (1) {
        static struct option long_options[] =
        {
            {"warmup_instructions", required_argument, 0, 'w'},
            {"simulation_instructions", required_argument, 0, 'i'},
            {"hide_heartbeat", no_argument, 0, 'h'},
            {"cloudsuite", no_argument, 0, 'c'},
            {"traces",  no_argument, 0, 't'},
            {0, 0, 0, 0}      
        };

        int option_index = 0;

        c = getopt_long_only(argc, argv, "wihsb", long_options, &option_index);

        // no more option characters
        if (c == -1)
            break;

        int traces_encountered = 0;

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
                traces_encountered = 1;
                break;
            default:
                abort();
        }

        if (traces_encountered == 1)
            break;
    }

    // consequences of knobs
    cout << "Warmup Instructions: " << warmup_instructions << endl;
    cout << "Simulation Instructions: " << simulation_instructions << endl;
    //cout << "Scramble Loads: " << (knob_scramble_loads ? "ture" : "false") << endl;
    cout << "Number of CPUs: " << NUM_CPUS << endl;
    cout << "LLC sets: " << LLC.NUM_SET << endl;
    cout << "LLC ways: " << LLC.NUM_WAY << endl;
    std::cout << "Off-chip DRAM Size: " << (DRAM_CHANNELS*DRAM_RANKS*DRAM_BANKS*DRAM_ROWS*DRAM_ROW_SIZE/1024) << " MB Channels: " << DRAM_CHANNELS << " Width: " << 8*DRAM_CHANNEL_WIDTH << "-bit Data Rate: " << DRAM_IO_FREQ << " MT/s" << std::endl;


    std::cout << std::endl;
    std::cout << "VirtualMemory physical capacity: " << std::size(vmem.ppage_free_list) * vmem.page_size;
    std::cout << " num_ppages: " << std::size(vmem.ppage_free_list) << std::endl;
    std::cout << "VirtualMemory page size: " << PAGE_SIZE << " log2_page_size: " << LOG2_PAGE_SIZE << std::endl;

    // end consequence of knobs

    // search through the argv for "-traces"
    int found_traces = 0;
    std::cout << std::endl;
    for (int i=0; i<argc; i++) {
        if (found_traces)
        {
            std::cout << "CPU " << traces.size() << " runs " << argv[i] << std::endl;

            traces.push_back(get_tracereader(argv[i], i, knob_cloudsuite));

            char *pch[100];
            int count_str = 0;
            pch[0] = strtok (argv[i], " /,.-");
            while (pch[count_str] != NULL) {
                //printf ("%s %d\n", pch[count_str], count_str);
                count_str++;
                pch[count_str] = strtok (NULL, " /,.-");
            }

            //printf("max count_str: %d\n", count_str);
            //printf("application: %s\n", pch[count_str-3]);

            int j = 0;
            while (pch[count_str-3][j] != '\0') {
                seed_number += pch[count_str-3][j];
                //printf("%c %d %d\n", pch[count_str-3][j], j, seed_number);
                j++;
            }

            if (traces.size() > NUM_CPUS) {
                printf("\n*** Too many traces for the configured number of cores ***\n\n");
                assert(0);
            }
        }
        else if(strcmp(argv[i],"-traces") == 0) {
            found_traces = 1;
        }
    }

    if (traces.size() != NUM_CPUS) {
        printf("\n*** Not enough traces for the configured number of cores ***\n\n");
        assert(0);
    }
    // end trace file setup

    srand(seed_number);
    champsim_seed = seed_number;

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
                // Phase complete
                if (!phase_complete[cpu->cpu] && (cpu->num_retired >= (cpu->begin_phase_instr + phase.length)))
                {
                    phase_complete.set(cpu->cpu);
                    for (auto op : operables)
                        op->end_phase(cpu->cpu);

                    std::cout << phase.name << " finished CPU " << cpu->cpu;
                    std::cout << " instructions: " << cpu->finish_phase_instr - cpu->begin_phase_instr;
                    std::cout << " cycles: " << cpu->finish_phase_cycle - cpu->begin_phase_cycle;
                    std::cout << " cumulative IPC: " << ((float) cpu->finish_phase_instr - cpu->begin_phase_instr) / (cpu->finish_phase_cycle - cpu->begin_phase_cycle);
                    std::cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << std::endl;
                }
            }
        }

        auto [elapsed_hour, elapsed_minute, elapsed_second] = elapsed_time();
        for (auto cpu : ooo_cpu)
        {
            std::cout << std::endl;
            std::cout << phase.name << " complete CPU " << cpu->cpu << " instructions: " << (cpu->num_retired - cpu->begin_phase_instr) << " cycles: " << (cpu->current_cycle - cpu->begin_phase_cycle);
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
            std::cout << "CPU " << cpu->cpu << " cumulative IPC: " << (float) (cpu->num_retired - cpu->begin_phase_instr) / (cpu->current_cycle - cpu->begin_phase_cycle);
            std::cout << " instructions: " << cpu->num_retired - cpu->begin_phase_instr << " cycles: " << cpu->current_cycle - cpu->begin_phase_cycle << endl;

            print_cache_stats(static_cast<CACHE*>(cpu->L1D_bus.lower_level)->NAME, cpu->cpu, static_cast<CACHE*>(cpu->L1D_bus.lower_level)->sim_stats.back());
            print_cache_stats(static_cast<CACHE*>(cpu->L1I_bus.lower_level)->NAME, cpu->cpu, static_cast<CACHE*>(cpu->L1I_bus.lower_level)->sim_stats.back());
            print_cache_stats(static_cast<CACHE*>(static_cast<CACHE*>(cpu->L1D_bus.lower_level)->lower_level)->NAME, cpu->cpu, static_cast<CACHE*>(static_cast<CACHE*>(cpu->L1D_bus.lower_level)->lower_level)->sim_stats.back());

            cpu->l1i_prefetcher_final_stats();
            static_cast<CACHE*>(cpu->L1D_bus.lower_level)->l1d_prefetcher_final_stats();
            static_cast<CACHE*>(static_cast<CACHE*>(cpu->L1D_bus.lower_level)->lower_level)->l2c_prefetcher_final_stats();

            print_cache_stats(LLC.NAME, cpu->cpu, LLC.sim_stats.back());
        }

        LLC.llc_prefetcher_final_stats();
    }

    std::cout << std::endl;
    std::cout << "Region of Interest Statistics" << std::endl;
    for (auto cpu : ooo_cpu)
    {
        std::cout << std::endl;
        std::cout << "CPU " << cpu->cpu << " cumulative IPC: " << ((float) (cpu->finish_phase_instr - cpu->begin_phase_instr) / (cpu->finish_phase_cycle - cpu->begin_phase_cycle));
        std::cout << " instructions: " << (cpu->finish_phase_instr - cpu->begin_phase_instr) << " cycles: " << (cpu->finish_phase_cycle - cpu->begin_phase_cycle);
        std::cout << endl;

        print_cache_stats(static_cast<CACHE*>(cpu->L1D_bus.lower_level)->NAME, cpu->cpu, static_cast<CACHE*>(cpu->L1D_bus.lower_level)->roi_stats.back());
        print_cache_stats(static_cast<CACHE*>(cpu->L1I_bus.lower_level)->NAME, cpu->cpu, static_cast<CACHE*>(cpu->L1I_bus.lower_level)->roi_stats.back());
        print_cache_stats(static_cast<CACHE*>(static_cast<CACHE*>(cpu->L1D_bus.lower_level)->lower_level)->NAME, cpu->cpu, static_cast<CACHE*>(static_cast<CACHE*>(cpu->L1D_bus.lower_level)->lower_level)->roi_stats.back());

        print_cache_stats(static_cast<CACHE*>(cpu->DTLB_bus.lower_level)->NAME, cpu->cpu, static_cast<CACHE*>(cpu->DTLB_bus.lower_level)->roi_stats.back());
        print_cache_stats(static_cast<CACHE*>(cpu->ITLB_bus.lower_level)->NAME, cpu->cpu, static_cast<CACHE*>(cpu->ITLB_bus.lower_level)->roi_stats.back());
        print_cache_stats(static_cast<CACHE*>(static_cast<CACHE*>(cpu->DTLB_bus.lower_level)->lower_level)->NAME, cpu->cpu, static_cast<CACHE*>(static_cast<CACHE*>(cpu->DTLB_bus.lower_level)->lower_level)->roi_stats.back());
        print_cache_stats(LLC.NAME, cpu->cpu, LLC.roi_stats.back());
    }

    for (auto cpu : ooo_cpu)
    {
        cpu->l1i_prefetcher_final_stats();
        static_cast<CACHE*>(cpu->L1D_bus.lower_level)->l1d_prefetcher_final_stats();
        static_cast<CACHE*>(static_cast<CACHE*>(cpu->L1D_bus.lower_level)->lower_level)->l2c_prefetcher_final_stats();
    }

    LLC.llc_prefetcher_final_stats();
    LLC.llc_replacement_final_stats();

    std::cout << std::endl;
    std::cout << "DRAM Statistics" << std::endl;
    for (unsigned i = 0; i < DRAM_CHANNELS; ++i)
        print_dram_channel_stats(i, DRAM.channels[i].sim_stats.back());

    for (auto cpu : ooo_cpu)
        print_cpu_stats(cpu->cpu, cpu->sim_stats.back(), cpu->begin_phase_instr, cpu->num_retired);

    return 0;
}
