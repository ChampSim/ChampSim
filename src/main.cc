#include <algorithm>
#include <array>
#include <bitset>
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

uint8_t warmup_complete[NUM_CPUS] = {},
        simulation_complete[NUM_CPUS] = {},
        all_warmup_complete = 0, 
        all_simulation_complete = 0,
        MAX_INSTR_DESTINATIONS = NUM_INSTR_DESTINATIONS,
        knob_cloudsuite = 0,
        knob_low_bandwidth = 0;

uint64_t warmup_instructions     = 1000000,
         simulation_instructions = 10000000,
         champsim_seed;

time_t start_time;

extern CACHE LLC;
extern MEMORY_CONTROLLER DRAM;
extern VirtualMemory vmem;
extern std::array<O3_CPU*, NUM_CPUS> ooo_cpu;
extern std::array<champsim::operable*, 7*NUM_CPUS+2> operables;

std::vector<tracereader*> traces;

void record_roi_stats(uint32_t cpu, CACHE *cache)
{
    std::copy(std::begin(cache->sim_hit[cpu]), std::end(cache->sim_hit[cpu]), std::begin(cache->roi_hit[cpu]));
    std::copy(std::begin(cache->sim_miss[cpu]), std::end(cache->sim_miss[cpu]), std::begin(cache->roi_miss[cpu]));

    cache->roi_pf_requested = cache->pf_requested;
    cache->roi_pf_issued    = cache->pf_issued;
    cache->roi_pf_fill      = cache->pf_fill;
    cache->roi_pf_useful    = cache->pf_useful;
    cache->roi_pf_useless   = cache->pf_useless;
}

void print_roi_stats(CACHE *cache)
{
    std::bitset<NUM_CPUS> active_cpus;
    uint64_t TOTAL_MISS = 0;

    for (std::size_t cpu = 0; cpu < NUM_CPUS; ++cpu)
    {
        uint64_t PER_CPU_HIT  = std::accumulate(std::begin(cache->roi_hit[cpu]), std::end(cache->roi_hit[cpu]), 0ll);
        uint64_t PER_CPU_MISS = std::accumulate(std::begin(cache->roi_miss[cpu]), std::end(cache->roi_miss[cpu]), 0ll);

        if (PER_CPU_HIT > 0 || PER_CPU_MISS > 0)
        {
            std::cout << "CPU" << cpu << " " << cache->NAME << " ROI TOTAL      ";
            std::cout << "  ACCESS: " << std::setw(10) << PER_CPU_HIT + PER_CPU_MISS;
            std::cout << "  HIT: "    << std::setw(10) << PER_CPU_HIT;
            std::cout << "  MISS: "   << std::setw(10) << PER_CPU_MISS;
            std::cout << std::endl;

            std::cout << "CPU" << cpu << " " << cache->NAME << " ROI LOAD       ";
            std::cout << "  ACCESS: " << std::setw(10) << cache->roi_hit[cpu][LOAD] + cache->roi_miss[cpu][LOAD];
            std::cout << "  HIT: "    << std::setw(10) << cache->roi_hit[cpu][LOAD];
            std::cout << "  MISS: "   << std::setw(10) << cache->roi_miss[cpu][LOAD];
            std::cout << std::endl;

            std::cout << "CPU" << cpu << " " << cache->NAME << " ROI RFO        ";
            std::cout << "  ACCESS: " << std::setw(10) << cache->roi_hit[cpu][RFO] + cache->roi_miss[cpu][RFO];
            std::cout << "  HIT: "    << std::setw(10) << cache->roi_hit[cpu][RFO];
            std::cout << "  MISS: "   << std::setw(10) << cache->roi_miss[cpu][RFO];
            std::cout << std::endl;

            std::cout << "CPU" << cpu << " " << cache->NAME << " ROI PREFETCH   ";
            std::cout << "  ACCESS: " << std::setw(10) << cache->roi_hit[cpu][PREFETCH] + cache->roi_miss[cpu][PREFETCH];
            std::cout << "  HIT: "    << std::setw(10) << cache->roi_hit[cpu][PREFETCH];
            std::cout << "  MISS: "   << std::setw(10) << cache->roi_miss[cpu][PREFETCH];
            std::cout << std::endl;

            std::cout << "CPU" << cpu << " " << cache->NAME << " ROI WRITEBACK  ";
            std::cout << "  ACCESS: " << std::setw(10) << cache->roi_hit[cpu][WRITEBACK] + cache->roi_miss[cpu][WRITEBACK];
            std::cout << "  HIT: "    << std::setw(10) << cache->roi_hit[cpu][WRITEBACK];
            std::cout << "  MISS: "   << std::setw(10) << cache->roi_miss[cpu][WRITEBACK];
            std::cout << std::endl;

            std::cout << "CPU" << cpu << " " << cache->NAME << " ROI TRANSLATION";
            std::cout << "  ACCESS: " << std::setw(10) << cache->roi_hit[cpu][TRANSLATION] + cache->roi_miss[cpu][TRANSLATION];
            std::cout << "  HIT: "    << std::setw(10) << cache->roi_hit[cpu][TRANSLATION];
            std::cout << "  MISS: "   << std::setw(10) << cache->roi_miss[cpu][TRANSLATION];
            std::cout << std::endl;

            active_cpus.set(cpu);
        }

        TOTAL_MISS += PER_CPU_MISS;
    }

    std::size_t cpu = 0;
    while (cpu < NUM_CPUS && !active_cpus.test(cpu)) ++cpu;

    if (active_cpus.count() == 1)
        std::cout << "CPU" << cpu << " ";
    std::cout << cache->NAME << " ROI PREFETCH ";
    std::cout << "  REQUESTED: " << std::setw(10) << cache->roi_pf_requested;
    std::cout << "  ISSUED: "    << std::setw(10) << cache->roi_pf_issued;
    std::cout << "  FILLED: "    << std::setw(10) << cache->roi_pf_fill;
    std::cout << "  USEFUL: "    << std::setw(10) << cache->roi_pf_useful;
    std::cout << "  USELESS: "   << std::setw(10) << cache->roi_pf_useless;
    std::cout << std::endl;

    if (active_cpus.count() == 1)
        std::cout << "CPU" << cpu << " ";
    std::cout << cache->NAME;
    std::cout << " AVERAGE MISS LATENCY: " << (1.0*(cache->total_miss_latency))/TOTAL_MISS << " cycles";
    std::cout << std::endl;
}

void print_sim_stats(CACHE *cache)
{
    std::bitset<NUM_CPUS> active_cpus;
    uint64_t TOTAL_MISS = 0;

    for (std::size_t cpu = 0; cpu < NUM_CPUS; ++cpu)
    {
        uint64_t PER_CPU_HIT  = std::accumulate(std::begin(cache->sim_hit[cpu]), std::end(cache->sim_hit[cpu]), 0ll);
        uint64_t PER_CPU_MISS = std::accumulate(std::begin(cache->sim_miss[cpu]), std::end(cache->sim_miss[cpu]), 0ll);

        if (PER_CPU_HIT > 0 || PER_CPU_MISS > 0)
        {
            std::cout << "CPU" << cpu << " " << cache->NAME << " SIM TOTAL    ";
            std::cout << "  ACCESS: " << std::setw(10) << PER_CPU_HIT + PER_CPU_MISS;
            std::cout << "  HIT: "    << std::setw(10) << PER_CPU_HIT;
            std::cout << "  MISS: "   << std::setw(10) << PER_CPU_MISS;
            std::cout << std::endl;

            std::cout << "CPU" << cpu << " " << cache->NAME << " SIM LOAD     ";
            std::cout << "  ACCESS: " << std::setw(10) << cache->sim_hit[cpu][LOAD] + cache->sim_miss[cpu][LOAD];
            std::cout << "  HIT: "    << std::setw(10) << cache->sim_hit[cpu][LOAD];
            std::cout << "  MISS: "   << std::setw(10) << cache->sim_miss[cpu][LOAD];
            std::cout << std::endl;

            std::cout << "CPU" << cpu << " " << cache->NAME << " SIM RFO      ";
            std::cout << "  ACCESS: " << std::setw(10) << cache->sim_hit[cpu][RFO] + cache->sim_miss[cpu][RFO];
            std::cout << "  HIT: "    << std::setw(10) << cache->sim_hit[cpu][RFO];
            std::cout << "  MISS: "   << std::setw(10) << cache->sim_miss[cpu][RFO];
            std::cout << std::endl;

            std::cout << "CPU" << cpu << " " << cache->NAME << " SIM PREFETCH ";
            std::cout << "  ACCESS: " << std::setw(10) << cache->sim_hit[cpu][PREFETCH] + cache->sim_miss[cpu][PREFETCH];
            std::cout << "  HIT: "    << std::setw(10) << cache->sim_hit[cpu][PREFETCH];
            std::cout << "  MISS: "   << std::setw(10) << cache->sim_miss[cpu][PREFETCH];
            std::cout << std::endl;

            std::cout << "CPU" << cpu << " " << cache->NAME << " SIM WRITEBACK";
            std::cout << "  ACCESS: " << std::setw(10) << cache->sim_hit[cpu][WRITEBACK] + cache->sim_miss[cpu][WRITEBACK];
            std::cout << "  HIT: "    << std::setw(10) << cache->sim_hit[cpu][WRITEBACK];
            std::cout << "  MISS: "   << std::setw(10) << cache->sim_miss[cpu][WRITEBACK];
            std::cout << std::endl;

            active_cpus.set(cpu);
        }

        TOTAL_MISS += PER_CPU_MISS;
    }

    std::size_t cpu = 0;
    while (cpu < NUM_CPUS && !active_cpus.test(cpu)) ++cpu;

    if (active_cpus.count() == 1)
        std::cout << "CPU" << cpu << " ";
    std::cout << cache->NAME << " SIM PREFETCH ";
    std::cout << "  REQUESTED: " << std::setw(10) << cache->pf_requested;
    std::cout << "  ISSUED: "    << std::setw(10) << cache->pf_issued;
    std::cout << "  FILLED: "    << std::setw(10) << cache->pf_fill;
    std::cout << "  USEFUL: "    << std::setw(10) << cache->pf_useful;
    std::cout << "  USELESS: "   << std::setw(10) << cache->pf_useless;
    std::cout << std::endl;

    if (active_cpus.count() == 1)
        std::cout << "CPU" << cpu << " ";
    std::cout << cache->NAME;
    std::cout << " AVERAGE MISS LATENCY: " << (1.0*(cache->total_miss_latency))/TOTAL_MISS << " cycles";
    std::cout << std::endl;
}

void print_branch_stats()
{
    for (uint32_t i=0; i<NUM_CPUS; i++) {
        cout << endl << "CPU " << i << " Branch Prediction Accuracy: ";
        cout << (100.0*(ooo_cpu[i]->num_branch - ooo_cpu[i]->branch_mispredictions)) / ooo_cpu[i]->num_branch;
        cout << "% MPKI: " << (1000.0*ooo_cpu[i]->branch_mispredictions)/(ooo_cpu[i]->num_retired - warmup_instructions);
	cout << " Average ROB Occupancy at Mispredict: " << (1.0*ooo_cpu[i]->total_rob_occupancy_at_branch_mispredict)/ooo_cpu[i]->branch_mispredictions << endl;

	/*
	cout << "Branch types" << endl;
	cout << "NOT_BRANCH: " << ooo_cpu[i]->total_branch_types[0] << " " << (100.0*ooo_cpu[i]->total_branch_types[0])/(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) << "%" << endl;
	cout << "BRANCH_DIRECT_JUMP: " << ooo_cpu[i]->total_branch_types[1] << " " << (100.0*ooo_cpu[i]->total_branch_types[1])/(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) << "%" << endl;
	cout << "BRANCH_INDIRECT: " << ooo_cpu[i]->total_branch_types[2] << " " << (100.0*ooo_cpu[i]->total_branch_types[2])/(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) << "%" << endl;
	cout << "BRANCH_CONDITIONAL: " << ooo_cpu[i]->total_branch_types[3] << " " << (100.0*ooo_cpu[i]->total_branch_types[3])/(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) << "%" << endl;
	cout << "BRANCH_DIRECT_CALL: " << ooo_cpu[i]->total_branch_types[4] << " " << (100.0*ooo_cpu[i]->total_branch_types[4])/(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) << "%" << endl;
	cout << "BRANCH_INDIRECT_CALL: " << ooo_cpu[i]->total_branch_types[5] << " " << (100.0*ooo_cpu[i]->total_branch_types[5])/(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) << "%" << endl;
	cout << "BRANCH_RETURN: " << ooo_cpu[i]->total_branch_types[6] << " " << (100.0*ooo_cpu[i]->total_branch_types[6])/(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) << "%" << endl;
	cout << "BRANCH_OTHER: " << ooo_cpu[i]->total_branch_types[7] << " " << (100.0*ooo_cpu[i]->total_branch_types[7])/(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) << "%" << endl << endl;
	*/

	cout << "Branch type MPKI" << endl;
	cout << "BRANCH_DIRECT_JUMP: " << (1000.0*ooo_cpu[i]->branch_type_misses[1]/(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
	cout << "BRANCH_INDIRECT: " << (1000.0*ooo_cpu[i]->branch_type_misses[2]/(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
	cout << "BRANCH_CONDITIONAL: " << (1000.0*ooo_cpu[i]->branch_type_misses[3]/(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
	cout << "BRANCH_DIRECT_CALL: " << (1000.0*ooo_cpu[i]->branch_type_misses[4]/(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
	cout << "BRANCH_INDIRECT_CALL: " << (1000.0*ooo_cpu[i]->branch_type_misses[5]/(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
	cout << "BRANCH_RETURN: " << (1000.0*ooo_cpu[i]->branch_type_misses[6]/(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl << endl;
    }
}

void print_dram_stats()
{
    uint64_t total_congested_cycle = 0;
    uint64_t total_congested_count = 0;
    for (uint32_t i=0; i<DRAM_CHANNELS; i++)
    {
        total_congested_cycle += DRAM.channels[i].dbus_cycle_congested;
        total_congested_count += DRAM.channels[i].dbus_count_congested;
    }

    std::cout << std::endl;
    std::cout << "DRAM Statistics" << std::endl;
    for (uint32_t i=0; i<DRAM_CHANNELS; i++) {
        std::cout << " CHANNEL " << i << std::endl;
        std::cout << " RQ ROW_BUFFER_HIT: " << std::setw(10) << DRAM.channels[i].RQ_ROW_BUFFER_HIT << "  ROW_BUFFER_MISS: " << std::setw(10) << DRAM.channels[i].RQ_ROW_BUFFER_MISS << std::endl;
        std::cout << " DBUS_CONGESTED: " << std::setw(10) << total_congested_count << std::endl;
        std::cout << " WQ ROW_BUFFER_HIT: " << std::setw(10) << DRAM.channels[i].WQ_ROW_BUFFER_HIT << "  ROW_BUFFER_MISS: " << std::setw(10) << DRAM.channels[i].WQ_ROW_BUFFER_MISS;
        std::cout << "  FULL: " << setw(10) << DRAM.channels[i].WQ_FULL << std::endl;
        std::cout << std::endl;
    }

    if (total_congested_count)
        cout << " AVG_CONGESTED_CYCLE: " << ((double)total_congested_cycle / total_congested_count) << endl;
    else
        cout << " AVG_CONGESTED_CYCLE: -" << endl;
}

void finish_warmup()
{
    uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time),
             elapsed_minute = elapsed_second / 60,
             elapsed_hour = elapsed_minute / 60;
    elapsed_minute -= elapsed_hour*60;
    elapsed_second -= (elapsed_hour*3600 + elapsed_minute*60);

    // reset core latency
    // note: since re-ordering he function calls in the main simulation loop, it's no longer necessary to add
    //       extra latency for scheduling and execution, unless you want these steps to take longer than 1 cycle.
    //PAGE_TABLE_LATENCY = 100;
    //SWAP_LATENCY = 100000;

    cout << endl;
    for (uint32_t i=0; i<NUM_CPUS; i++) {
        cout << "Warmup complete CPU " << i << " instructions: " << ooo_cpu[i]->num_retired << " cycles: " << ooo_cpu[i]->current_cycle;
        cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;

        ooo_cpu[i]->begin_sim_cycle = ooo_cpu[i]->current_cycle; 
        ooo_cpu[i]->begin_sim_instr = ooo_cpu[i]->num_retired;

        // reset branch stats
        ooo_cpu[i]->num_branch = 0;
        ooo_cpu[i]->branch_mispredictions = 0;
	ooo_cpu[i]->total_rob_occupancy_at_branch_mispredict = 0;

	for(uint32_t j=0; j<8; j++)
	  {
	    ooo_cpu[i]->total_branch_types[j] = 0;
	    ooo_cpu[i]->branch_type_misses[j] = 0;
	  }
	
        static_cast<CACHE*>(ooo_cpu[i]->L1I_bus.lower_level)->reset_stats();
        static_cast<CACHE*>(ooo_cpu[i]->L1D_bus.lower_level)->reset_stats();
        static_cast<CACHE*>(static_cast<CACHE*>(ooo_cpu[i]->L1D_bus.lower_level)->lower_level)->reset_stats(); //L2C

        static_cast<CACHE*>(ooo_cpu[i]->ITLB_bus.lower_level)->reset_stats();
        static_cast<CACHE*>(ooo_cpu[i]->DTLB_bus.lower_level)->reset_stats();
        static_cast<CACHE*>(static_cast<CACHE*>(ooo_cpu[i]->DTLB_bus.lower_level)->lower_level)->reset_stats(); //L2C
    }
    LLC.reset_stats();
    cout << endl;

    // reset DRAM stats
    for (uint32_t i=0; i<DRAM_CHANNELS; i++) {
        DRAM.channels[i].WQ_ROW_BUFFER_HIT = 0;
        DRAM.channels[i].WQ_ROW_BUFFER_MISS = 0;
        DRAM.channels[i].RQ_ROW_BUFFER_HIT = 0;
        DRAM.channels[i].RQ_ROW_BUFFER_MISS = 0;
    }
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

int main(int argc, char** argv)
{
	// interrupt signal hanlder
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = signal_handler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;
	sigaction(SIGINT, &sigIntHandler, NULL);

    cout << endl << "*** ChampSim Multicore Out-of-Order Simulator ***" << endl << endl;

    // initialize knobs
    uint8_t show_heartbeat = 1;

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

    for (int i=0; i<NUM_CPUS; i++) {
        static_cast<CACHE*>(ooo_cpu.at(i)->L1I_bus.lower_level)->l1i_prefetcher_cache_operate = cpu_l1i_prefetcher_cache_operate;
        static_cast<CACHE*>(ooo_cpu.at(i)->L1I_bus.lower_level)->l1i_prefetcher_cache_fill = cpu_l1i_prefetcher_cache_fill;
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

    // simulation entry point
    start_time = time(NULL);
    while (std::any_of(std::begin(simulation_complete), std::end(simulation_complete), std::logical_not<uint8_t>())) {

        uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time),
                 elapsed_minute = elapsed_second / 60,
                 elapsed_hour = elapsed_minute / 60;
        elapsed_minute -= elapsed_hour*60;
        elapsed_second -= (elapsed_hour*3600 + elapsed_minute*60);

        for (auto op : operables)
        {
            op->_operate();
        }
        std::sort(std::begin(operables), std::end(operables), champsim::by_next_operate());

        for (std::size_t i = 0; i < ooo_cpu.size(); ++i)
        {
            // read from trace
            while (ooo_cpu[i]->fetch_stall == 0 && ooo_cpu[i]->instrs_to_read_this_cycle > 0)
            {
                ooo_cpu[i]->init_instruction(traces[i]->get());
            }

            // heartbeat information
            if (show_heartbeat && (ooo_cpu[i]->num_retired >= ooo_cpu[i]->next_print_instruction)) {
                float cumulative_ipc;
                if (warmup_complete[i])
                    cumulative_ipc = (1.0*(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) / (ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle);
                else
                    cumulative_ipc = (1.0*ooo_cpu[i]->num_retired) / ooo_cpu[i]->current_cycle;
                float heartbeat_ipc = (1.0*ooo_cpu[i]->num_retired - ooo_cpu[i]->last_sim_instr) / (ooo_cpu[i]->current_cycle - ooo_cpu[i]->last_sim_cycle);

                cout << "Heartbeat CPU " << i << " instructions: " << ooo_cpu[i]->num_retired << " cycles: " << ooo_cpu[i]->current_cycle;
                cout << " heartbeat IPC: " << heartbeat_ipc << " cumulative IPC: " << cumulative_ipc; 
                cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;
                ooo_cpu[i]->next_print_instruction += STAT_PRINTING_PERIOD;

                ooo_cpu[i]->last_sim_instr = ooo_cpu[i]->num_retired;
                ooo_cpu[i]->last_sim_cycle = ooo_cpu[i]->current_cycle;
            }

            // check for warmup
            // warmup complete
            if ((warmup_complete[i] == 0) && (ooo_cpu[i]->num_retired > warmup_instructions)) {
                warmup_complete[i] = 1;
                all_warmup_complete++;
            }
            if (all_warmup_complete == NUM_CPUS) { // this part is called only once when all cores are warmed up
                all_warmup_complete++;
                finish_warmup();
            }

            // simulation complete
            if ((all_warmup_complete > NUM_CPUS) && (simulation_complete[i] == 0) && (ooo_cpu[i]->num_retired >= (ooo_cpu[i]->begin_sim_instr + simulation_instructions))) {
                simulation_complete[i] = 1;
                ooo_cpu[i]->finish_sim_instr = ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr;
                ooo_cpu[i]->finish_sim_cycle = ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle;

                cout << "Finished CPU " << i << " instructions: " << ooo_cpu[i]->finish_sim_instr << " cycles: " << ooo_cpu[i]->finish_sim_cycle;
                cout << " cumulative IPC: " << ((float) ooo_cpu[i]->finish_sim_instr / ooo_cpu[i]->finish_sim_cycle);
                cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;

                record_roi_stats(i, static_cast<CACHE*>(ooo_cpu[i]->L1D_bus.lower_level));
                record_roi_stats(i, static_cast<CACHE*>(ooo_cpu[i]->L1I_bus.lower_level));
                record_roi_stats(i, static_cast<CACHE*>(static_cast<CACHE*>(ooo_cpu[i]->L1D_bus.lower_level)->lower_level));
                record_roi_stats(i, &LLC);

                record_roi_stats(i, static_cast<CACHE*>(ooo_cpu[i]->DTLB_bus.lower_level));
                record_roi_stats(i, static_cast<CACHE*>(ooo_cpu[i]->ITLB_bus.lower_level));
                record_roi_stats(i, static_cast<CACHE*>(static_cast<CACHE*>(ooo_cpu[i]->DTLB_bus.lower_level)->lower_level));
            }
        }
    }

    uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time),
             elapsed_minute = elapsed_second / 60,
             elapsed_hour = elapsed_minute / 60;
    elapsed_minute -= elapsed_hour*60;
    elapsed_second -= (elapsed_hour*3600 + elapsed_minute*60);
    
    std::cout << std::endl;
    std::cout << "ChampSim completed all CPUs" << std::endl << std::endl;;

    // In a multicore simulation, print full-time statistics
    // In a single-core environment, the full simulation and the ROI are the same
    if (NUM_CPUS > 1) {
        std::cout << "Total Simulation Statistics (not including warmup)" << std::endl << std::endl;

        for (std::size_t i=0; i<NUM_CPUS; ++i)
        {
            std::cout << "CPU" << i << " SIM cumulative IPC: " << (float) (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) / (ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle);
            std::cout << " instructions: " << ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr;
            std::cout << " cycles: " << ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle << std::endl;

            print_sim_stats(static_cast<CACHE*>(ooo_cpu[i]->L1D_bus.lower_level));
            print_sim_stats(static_cast<CACHE*>(ooo_cpu[i]->L1I_bus.lower_level));
            print_sim_stats(static_cast<CACHE*>(static_cast<CACHE*>(ooo_cpu[i]->L1D_bus.lower_level)->lower_level));

            print_roi_stats(static_cast<CACHE*>(ooo_cpu[i]->DTLB_bus.lower_level));
            print_roi_stats(static_cast<CACHE*>(ooo_cpu[i]->ITLB_bus.lower_level));
            print_roi_stats(static_cast<CACHE*>(static_cast<CACHE*>(ooo_cpu[i]->DTLB_bus.lower_level)->lower_level));

            std::cout << std::endl;
        }
        print_sim_stats(&LLC);
    }

    std::cout << std::endl;
    std::cout << "Region of Interest Statistics" << std::endl << std::endl;
    for (std::size_t i=0; i<NUM_CPUS; ++i)
    {
        std::cout << "CPU" << i << " ROI cumulative IPC: " << ((float) ooo_cpu[i]->finish_sim_instr / ooo_cpu[i]->finish_sim_cycle);
        std::cout << " instructions: " << ooo_cpu[i]->finish_sim_instr;
        std::cout << " cycles: " << ooo_cpu[i]->finish_sim_cycle << std::endl;

        print_roi_stats(static_cast<CACHE*>(ooo_cpu[i]->L1D_bus.lower_level));
        print_roi_stats(static_cast<CACHE*>(ooo_cpu[i]->L1I_bus.lower_level));
        print_roi_stats(static_cast<CACHE*>(static_cast<CACHE*>(ooo_cpu[i]->L1D_bus.lower_level)->lower_level));

        print_roi_stats(static_cast<CACHE*>(ooo_cpu[i]->DTLB_bus.lower_level));
        print_roi_stats(static_cast<CACHE*>(ooo_cpu[i]->ITLB_bus.lower_level));
        print_roi_stats(static_cast<CACHE*>(static_cast<CACHE*>(ooo_cpu[i]->DTLB_bus.lower_level)->lower_level));

        std::cout << std::endl;
    }
    print_roi_stats(&LLC);

    for (uint32_t i=0; i<NUM_CPUS; i++)
    {
        std::cout << std::endl;
        ooo_cpu[i]->l1i_prefetcher_final_stats();
        static_cast<CACHE*>(ooo_cpu[i]->L1D_bus.lower_level)->l1d_prefetcher_final_stats();
        static_cast<CACHE*>(static_cast<CACHE*>(ooo_cpu[i]->L1D_bus.lower_level)->lower_level)->l2c_prefetcher_final_stats();
    }

    std::cout << std::endl;
    LLC.llc_replacement_final_stats();
    print_dram_stats();
    print_branch_stats();

    return 0;
}
