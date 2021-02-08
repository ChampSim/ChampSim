#include <array>
#include <getopt.h>
#include <fstream>
#include <iomanip>
#include <signal.h>
#include <vector>

#include "champsim_constants.h"
#include "dram_controller.h"
#include "ooo_cpu.h"
#include "vmem.h"
#include "tracereader.h"

#define DRAM_SIZE (DRAM_CHANNELS*DRAM_RANKS*DRAM_BANKS*DRAM_ROWS*DRAM_ROW_SIZE/1024)

uint8_t warmup_complete[NUM_CPUS], 
        simulation_complete[NUM_CPUS], 
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
extern std::vector<O3_CPU> ooo_cpu;

extern uint64_t current_core_cycle[NUM_CPUS];

std::vector<tracereader*> traces;

void record_roi_stats(uint32_t cpu, CACHE *cache)
{
    for (uint32_t i=0; i<NUM_TYPES; i++) {
        cache->roi_access[cpu][i] = cache->sim_access[cpu][i];
        cache->roi_hit[cpu][i] = cache->sim_hit[cpu][i];
        cache->roi_miss[cpu][i] = cache->sim_miss[cpu][i];
    }
}

void print_roi_stats(uint32_t cpu, CACHE *cache)
{
    uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0, TOTAL_MISS = 0;

    for (uint32_t i=0; i<NUM_TYPES; i++) {
        TOTAL_ACCESS += cache->roi_access[cpu][i];
        TOTAL_HIT += cache->roi_hit[cpu][i];
        TOTAL_MISS += cache->roi_miss[cpu][i];
    }

    cout << cache->NAME;
    cout << " TOTAL     ACCESS: " << setw(10) << TOTAL_ACCESS << "  HIT: " << setw(10) << TOTAL_HIT << "  MISS: " << setw(10) << TOTAL_MISS << endl;

    cout << cache->NAME;
    cout << " LOAD      ACCESS: " << setw(10) << cache->roi_access[cpu][0] << "  HIT: " << setw(10) << cache->roi_hit[cpu][0] << "  MISS: " << setw(10) << cache->roi_miss[cpu][0] << endl;

    cout << cache->NAME;
    cout << " RFO       ACCESS: " << setw(10) << cache->roi_access[cpu][1] << "  HIT: " << setw(10) << cache->roi_hit[cpu][1] << "  MISS: " << setw(10) << cache->roi_miss[cpu][1] << endl;

    cout << cache->NAME;
    cout << " PREFETCH  ACCESS: " << setw(10) << cache->roi_access[cpu][2] << "  HIT: " << setw(10) << cache->roi_hit[cpu][2] << "  MISS: " << setw(10) << cache->roi_miss[cpu][2] << endl;

    cout << cache->NAME;
    cout << " WRITEBACK ACCESS: " << setw(10) << cache->roi_access[cpu][3] << "  HIT: " << setw(10) << cache->roi_hit[cpu][3] << "  MISS: " << setw(10) << cache->roi_miss[cpu][3] << endl;

    cout << cache->NAME;
    cout << " PREFETCH  REQUESTED: " << setw(10) << cache->pf_requested << "  ISSUED: " << setw(10) << cache->pf_issued;
    cout << "  USEFUL: " << setw(10) << cache->pf_useful << "  USELESS: " << setw(10) << cache->pf_useless << "  POLLUTING: " << setw(10) << cache->pf_polluting << endl;

    cout << cache->NAME;
    cout << " AVERAGE MISS LATENCY: " << (1.0*(cache->total_miss_latency))/TOTAL_MISS << " cycles" << endl;
    //cout << " AVERAGE MISS LATENCY: " << (cache->total_miss_latency)/TOTAL_MISS << " cycles " << cache->total_miss_latency << "/" << TOTAL_MISS<< endl;
}

void print_sim_stats(uint32_t cpu, CACHE *cache)
{
    uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0, TOTAL_MISS = 0;

    for (uint32_t i=0; i<NUM_TYPES; i++) {
        TOTAL_ACCESS += cache->sim_access[cpu][i];
        TOTAL_HIT += cache->sim_hit[cpu][i];
        TOTAL_MISS += cache->sim_miss[cpu][i];
    }

    cout << cache->NAME;
    cout << " TOTAL     ACCESS: " << setw(10) << TOTAL_ACCESS << "  HIT: " << setw(10) << TOTAL_HIT << "  MISS: " << setw(10) << TOTAL_MISS << endl;

    cout << cache->NAME;
    cout << " LOAD      ACCESS: " << setw(10) << cache->sim_access[cpu][0] << "  HIT: " << setw(10) << cache->sim_hit[cpu][0] << "  MISS: " << setw(10) << cache->sim_miss[cpu][0] << endl;

    cout << cache->NAME;
    cout << " RFO       ACCESS: " << setw(10) << cache->sim_access[cpu][1] << "  HIT: " << setw(10) << cache->sim_hit[cpu][1] << "  MISS: " << setw(10) << cache->sim_miss[cpu][1] << endl;

    cout << cache->NAME;
    cout << " PREFETCH  ACCESS: " << setw(10) << cache->sim_access[cpu][2] << "  HIT: " << setw(10) << cache->sim_hit[cpu][2] << "  MISS: " << setw(10) << cache->sim_miss[cpu][2] << endl;

    cout << cache->NAME;
    cout << " WRITEBACK ACCESS: " << setw(10) << cache->sim_access[cpu][3] << "  HIT: " << setw(10) << cache->sim_hit[cpu][3] << "  MISS: " << setw(10) << cache->sim_miss[cpu][3] << endl;
}

void print_branch_stats()
{
    for (uint32_t i=0; i<NUM_CPUS; i++) {
        cout << endl << "CPU " << i << " Branch Prediction Accuracy: ";
        cout << (100.0*(ooo_cpu[i].num_branch - ooo_cpu[i].branch_mispredictions)) / ooo_cpu[i].num_branch;
        cout << "% MPKI: " << (1000.0*ooo_cpu[i].branch_mispredictions)/(ooo_cpu[i].num_retired - ooo_cpu[i].warmup_instructions);
	cout << " Average ROB Occupancy at Mispredict: " << (1.0*ooo_cpu[i].total_rob_occupancy_at_branch_mispredict)/ooo_cpu[i].branch_mispredictions << endl;

	/*
	cout << "Branch types" << endl;
	cout << "NOT_BRANCH: " << ooo_cpu[i].total_branch_types[0] << " " << (100.0*ooo_cpu[i].total_branch_types[0])/(ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr) << "%" << endl;
	cout << "BRANCH_DIRECT_JUMP: " << ooo_cpu[i].total_branch_types[1] << " " << (100.0*ooo_cpu[i].total_branch_types[1])/(ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr) << "%" << endl;
	cout << "BRANCH_INDIRECT: " << ooo_cpu[i].total_branch_types[2] << " " << (100.0*ooo_cpu[i].total_branch_types[2])/(ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr) << "%" << endl;
	cout << "BRANCH_CONDITIONAL: " << ooo_cpu[i].total_branch_types[3] << " " << (100.0*ooo_cpu[i].total_branch_types[3])/(ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr) << "%" << endl;
	cout << "BRANCH_DIRECT_CALL: " << ooo_cpu[i].total_branch_types[4] << " " << (100.0*ooo_cpu[i].total_branch_types[4])/(ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr) << "%" << endl;
	cout << "BRANCH_INDIRECT_CALL: " << ooo_cpu[i].total_branch_types[5] << " " << (100.0*ooo_cpu[i].total_branch_types[5])/(ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr) << "%" << endl;
	cout << "BRANCH_RETURN: " << ooo_cpu[i].total_branch_types[6] << " " << (100.0*ooo_cpu[i].total_branch_types[6])/(ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr) << "%" << endl;
	cout << "BRANCH_OTHER: " << ooo_cpu[i].total_branch_types[7] << " " << (100.0*ooo_cpu[i].total_branch_types[7])/(ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr) << "%" << endl << endl;
	*/

	cout << "Branch type MPKI" << endl;
	cout << "BRANCH_DIRECT_JUMP: " << (1000.0*ooo_cpu[i].branch_type_misses[1]/(ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr)) << endl;
	cout << "BRANCH_INDIRECT: " << (1000.0*ooo_cpu[i].branch_type_misses[2]/(ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr)) << endl;
	cout << "BRANCH_CONDITIONAL: " << (1000.0*ooo_cpu[i].branch_type_misses[3]/(ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr)) << endl;
	cout << "BRANCH_DIRECT_CALL: " << (1000.0*ooo_cpu[i].branch_type_misses[4]/(ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr)) << endl;
	cout << "BRANCH_INDIRECT_CALL: " << (1000.0*ooo_cpu[i].branch_type_misses[5]/(ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr)) << endl;
	cout << "BRANCH_RETURN: " << (1000.0*ooo_cpu[i].branch_type_misses[6]/(ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr)) << endl << endl;
    }
}

void print_dram_stats()
{
    cout << endl;
    cout << "DRAM Statistics" << endl;
    for (uint32_t i=0; i<DRAM_CHANNELS; i++) {
        cout << " CHANNEL " << i << endl;
        cout << " RQ ROW_BUFFER_HIT: " << setw(10) << DRAM.RQ[i].ROW_BUFFER_HIT << "  ROW_BUFFER_MISS: " << setw(10) << DRAM.RQ[i].ROW_BUFFER_MISS << endl;
        cout << " DBUS_CONGESTED: " << setw(10) << DRAM.dbus_congested[NUM_TYPES][NUM_TYPES] << endl; 
        cout << " WQ ROW_BUFFER_HIT: " << setw(10) << DRAM.WQ[i].ROW_BUFFER_HIT << "  ROW_BUFFER_MISS: " << setw(10) << DRAM.WQ[i].ROW_BUFFER_MISS;
        cout << "  FULL: " << setw(10) << DRAM.WQ[i].FULL << endl; 
        cout << endl;
    }

    uint64_t total_congested_cycle = 0;
    for (uint32_t i=0; i<DRAM_CHANNELS; i++)
        total_congested_cycle += DRAM.dbus_cycle_congested[i];
    if (DRAM.dbus_congested[NUM_TYPES][NUM_TYPES])
        cout << " AVG_CONGESTED_CYCLE: " << (total_congested_cycle / DRAM.dbus_congested[NUM_TYPES][NUM_TYPES]) << endl;
    else
        cout << " AVG_CONGESTED_CYCLE: -" << endl;
}

void reset_cache_stats(uint32_t cpu, CACHE *cache)
{
    for (uint32_t i=0; i<NUM_TYPES; i++) {
        cache->sim_access[cpu][i] = 0;
        cache->sim_hit[cpu][i] = 0;
        cache->sim_miss[cpu][i] = 0;
    }

    cache->pf_requested = 0;
    cache->pf_issued = 0;
    cache->pf_useful = 0;
    cache->pf_useless = 0;
    cache->pf_fill = 0;


    cache->total_miss_latency = 0;

    cache->RQ_ACCESS = 0;
    cache->RQ_MERGED = 0;
    cache->RQ_TO_CACHE = 0;

    cache->WQ_ACCESS = 0;
    cache->WQ_MERGED = 0;
    cache->WQ_TO_CACHE = 0;
    cache->WQ_FORWARD = 0;
    cache->WQ_FULL = 0;
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
        cout << "Warmup complete CPU " << i << " instructions: " << ooo_cpu[i].num_retired << " cycles: " << current_core_cycle[i];
        cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;

        ooo_cpu[i].begin_sim_cycle = current_core_cycle[i]; 
        ooo_cpu[i].begin_sim_instr = ooo_cpu[i].num_retired;

        // reset branch stats
        ooo_cpu[i].num_branch = 0;
        ooo_cpu[i].branch_mispredictions = 0;
	ooo_cpu[i].total_rob_occupancy_at_branch_mispredict = 0;

	for(uint32_t j=0; j<8; j++)
	  {
	    ooo_cpu[i].total_branch_types[j] = 0;
	    ooo_cpu[i].branch_type_misses[j] = 0;
	  }
	
        reset_cache_stats(i, &ooo_cpu[i].L1I);
        reset_cache_stats(i, &ooo_cpu[i].L1D);
        reset_cache_stats(i, &ooo_cpu[i].L2C);
        reset_cache_stats(i, &LLC);
    }
    cout << endl;

    // reset DRAM stats
    for (uint32_t i=0; i<DRAM_CHANNELS; i++) {
        DRAM.RQ[i].ROW_BUFFER_HIT = 0;
        DRAM.RQ[i].ROW_BUFFER_MISS = 0;
        DRAM.WQ[i].ROW_BUFFER_HIT = 0;
        DRAM.WQ[i].ROW_BUFFER_MISS = 0;
    }
}

void print_deadlock(uint32_t i)
{
    cout << "DEADLOCK! CPU " << i << " instr_id: " << ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].instr_id;
    cout << " translated: " << +ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].translated;
    cout << " fetched: " << +ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].fetched;
    cout << " scheduled: " << +ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].scheduled;
    cout << " executed: " << +ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].executed;
    cout << " is_memory: " << +ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].is_memory;
    cout << " event: " << ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].event_cycle;
    cout << " current: " << current_core_cycle[i] << endl;

    // print LQ entry
    cout << endl << "Load Queue Entry" << endl;
    for (uint32_t j=0; j<LQ_SIZE; j++) {
        cout << "[LQ] entry: " << j << " instr_id: " << ooo_cpu[i].LQ.entry[j].instr_id << " address: " << hex << ooo_cpu[i].LQ.entry[j].physical_address << dec << " translated: " << +ooo_cpu[i].LQ.entry[j].translated << " fetched: " << +ooo_cpu[i].LQ.entry[i].fetched << endl;
    }

    // print SQ entry
    cout << endl << "Store Queue Entry" << endl;
    for (uint32_t j=0; j<SQ_SIZE; j++) {
        cout << "[SQ] entry: " << j << " instr_id: " << ooo_cpu[i].SQ.entry[j].instr_id << " address: " << hex << ooo_cpu[i].SQ.entry[j].physical_address << dec << " translated: " << +ooo_cpu[i].SQ.entry[j].translated << " fetched: " << +ooo_cpu[i].SQ.entry[i].fetched << endl;
    }

    // print L1D MSHR entry
    std::cout << std::endl << "L1D MSHR Entry" << std::endl;
    std::size_t j = 0;
    for (PACKET &entry : ooo_cpu[i].L1D.MSHR) {
        std::cout << "[L1D MSHR] entry: " << j << " instr_id: " << entry.instr_id << " rob_index: " << entry.rob_index;
        std::cout << " address: " << std::hex << entry.address << " full_addr: " << entry.full_addr << std::dec << " type: " << +entry.type;
        std::cout << " fill_level: " << entry.fill_level << " lq_index: " << entry.lq_index << " sq_index: " << entry.sq_index << " event_cycle: " << entry.event_cycle << std::endl;
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
  ooo_cpu[cpu_num].l1i_prefetcher_cache_operate(v_addr, cache_hit, prefetch_hit);
}

void cpu_l1i_prefetcher_cache_fill(uint32_t cpu_num, uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr)
{
  ooo_cpu[cpu_num].l1i_prefetcher_cache_fill(addr, set, way, prefetch, evicted_addr);
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
            {"low_bandwidth",  no_argument, 0, 'b'},
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
            case 'b':
                knob_low_bandwidth = 1;
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
    cout << "LLC sets: " << LLC_SET << endl;
    cout << "LLC ways: " << LLC_WAY << endl;

    if (knob_low_bandwidth)
        DRAM_MTPS = DRAM_IO_FREQ/4;
    else
        DRAM_MTPS = DRAM_IO_FREQ;

    // DRAM access latency
    tRP  = (uint32_t)((1.0 * tRP_DRAM_NANOSECONDS  * CPU_FREQ) / 1000); 
    tRCD = (uint32_t)((1.0 * tRCD_DRAM_NANOSECONDS * CPU_FREQ) / 1000); 
    tCAS = (uint32_t)((1.0 * tCAS_DRAM_NANOSECONDS * CPU_FREQ) / 1000); 

    // default: 16 = (64 / 8) * (3200 / 1600)
    // it takes 16 CPU cycles to tranfser 64B cache block on a 8B (64-bit) bus 
    // note that dram burst length = BLOCK_SIZE/DRAM_CHANNEL_WIDTH
    DRAM_DBUS_RETURN_TIME = (BLOCK_SIZE / DRAM_CHANNEL_WIDTH) * (CPU_FREQ / DRAM_MTPS);

    printf("Off-chip DRAM Size: %u MB Channels: %u Width: %u-bit Data Rate: %u MT/s\n",
            DRAM_SIZE, DRAM_CHANNELS, 8*DRAM_CHANNEL_WIDTH, DRAM_MTPS);

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

    ooo_cpu.reserve(NUM_CPUS);

    for (int i=0; i<NUM_CPUS; i++) {
        ooo_cpu.emplace_back(i, warmup_instructions, simulation_instructions);
        ooo_cpu.at(i).L1I.l1i_prefetcher_cache_operate = cpu_l1i_prefetcher_cache_operate;
        ooo_cpu.at(i).L1I.l1i_prefetcher_cache_fill = cpu_l1i_prefetcher_cache_fill;
        ooo_cpu.at(i).L2C.lower_level = &LLC;

        // SHARED CACHE
        LLC.cache_type = IS_LLC;
        LLC.fill_level = FILL_LLC;
        LLC.lower_level = &DRAM;

        using namespace std::placeholders;
        LLC.find_victim = std::bind(&CACHE::llc_find_victim, &LLC, _1, _2, _3, _4, _5, _6, _7);
        LLC.update_replacement_state = std::bind(&CACHE::llc_update_replacement_state, &LLC, _1, _2, _3, _4, _5, _6, _7, _8);
        LLC.replacement_final_stats = std::bind(&CACHE::lru_final_stats, &LLC);

        // OFF-CHIP DRAM
        DRAM.fill_level = FILL_DRAM;
        for (uint32_t i=0; i<DRAM_CHANNELS; i++) {
            DRAM.RQ[i].is_RQ = 1;
            DRAM.WQ[i].is_WQ = 1;
        }

        warmup_complete[i] = 0;
        //all_warmup_complete = NUM_CPUS;
        simulation_complete[i] = 0;
        current_core_cycle[i] = 0;
    }

    LLC.llc_initialize_replacement();
    LLC.llc_prefetcher_initialize();

    // simulation entry point
    start_time = time(NULL);
    uint8_t run_simulation = 1;
    while (run_simulation) {

        uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time),
                 elapsed_minute = elapsed_second / 60,
                 elapsed_hour = elapsed_minute / 60;
        elapsed_minute -= elapsed_hour*60;
        elapsed_second -= (elapsed_hour*3600 + elapsed_minute*60);

        for (int i=0; i<NUM_CPUS; i++) {
            // proceed one cycle
            current_core_cycle[i]++;

            //cout << "Trying to process instr_id: " << ooo_cpu[i].instr_unique_id << " fetch_stall: " << +ooo_cpu[i].fetch_stall;

	      // retire
	      ooo_cpu[i].retire_rob();
	      // finalize execution
	      ooo_cpu[i].complete_inflight_instruction();
	      // execute instructions
	      ooo_cpu[i].execute_instruction();
	      // schedule instructions
	      ooo_cpu[i].schedule_instruction();
	      // finalize memory transactions
	      ooo_cpu[i].handle_memory_return();
	      // execute memory transactions
	      ooo_cpu[i].execute_memory_instruction();
	      // schedule memory transactions
	      ooo_cpu[i].schedule_memory_instruction();
	      // dispatch
	      ooo_cpu[i].dispatch_instruction();
	      // decode
	      ooo_cpu[i].decode_instruction();
	      // fetch
	      ooo_cpu[i].fetch_instruction();
	      
	      // read from trace
	      if (!ooo_cpu[i].IFETCH_BUFFER.full() && (ooo_cpu[i].fetch_stall == 0))
                {
		  while(ooo_cpu[i].init_instruction(traces[i]->get()));
                }

            // heartbeat information
            if (show_heartbeat && (ooo_cpu[i].num_retired >= ooo_cpu[i].next_print_instruction)) {
                float cumulative_ipc;
                if (warmup_complete[i])
                    cumulative_ipc = (1.0*(ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr)) / (current_core_cycle[i] - ooo_cpu[i].begin_sim_cycle);
                else
                    cumulative_ipc = (1.0*ooo_cpu[i].num_retired) / current_core_cycle[i];
                float heartbeat_ipc = (1.0*ooo_cpu[i].num_retired - ooo_cpu[i].last_sim_instr) / (current_core_cycle[i] - ooo_cpu[i].last_sim_cycle);

                cout << "Heartbeat CPU " << i << " instructions: " << ooo_cpu[i].num_retired << " cycles: " << current_core_cycle[i];
                cout << " heartbeat IPC: " << heartbeat_ipc << " cumulative IPC: " << cumulative_ipc; 
                cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;
                ooo_cpu[i].next_print_instruction += STAT_PRINTING_PERIOD;

                ooo_cpu[i].last_sim_instr = ooo_cpu[i].num_retired;
                ooo_cpu[i].last_sim_cycle = current_core_cycle[i];
            }

            // check for deadlock
            if (ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].ip && (ooo_cpu[i].ROB.entry[ooo_cpu[i].ROB.head].event_cycle + DEADLOCK_CYCLE) <= current_core_cycle[i])
                print_deadlock(i);

            // check for warmup
            // warmup complete
            if ((warmup_complete[i] == 0) && (ooo_cpu[i].num_retired > warmup_instructions)) {
                warmup_complete[i] = 1;
                all_warmup_complete++;
            }
            if (all_warmup_complete == NUM_CPUS) { // this part is called only once when all cores are warmed up
                all_warmup_complete++;
                finish_warmup();
            }

            /*
            if (all_warmup_complete == 0) { 
                all_warmup_complete = 1;
                finish_warmup();
            }
            if (ooo_cpu[1].num_retired > 0)
                warmup_complete[1] = 1;
            */
            
            // simulation complete
            if ((all_warmup_complete > NUM_CPUS) && (simulation_complete[i] == 0) && (ooo_cpu[i].num_retired >= (ooo_cpu[i].begin_sim_instr + ooo_cpu[i].simulation_instructions))) {
                simulation_complete[i] = 1;
                ooo_cpu[i].finish_sim_instr = ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr;
                ooo_cpu[i].finish_sim_cycle = current_core_cycle[i] - ooo_cpu[i].begin_sim_cycle;

                cout << "Finished CPU " << i << " instructions: " << ooo_cpu[i].finish_sim_instr << " cycles: " << ooo_cpu[i].finish_sim_cycle;
                cout << " cumulative IPC: " << ((float) ooo_cpu[i].finish_sim_instr / ooo_cpu[i].finish_sim_cycle);
                cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;

                record_roi_stats(i, &ooo_cpu[i].L1D);
                record_roi_stats(i, &ooo_cpu[i].L1I);
                record_roi_stats(i, &ooo_cpu[i].L2C);
                record_roi_stats(i, &LLC);

                all_simulation_complete++;
            }

            if (all_simulation_complete == NUM_CPUS)
                run_simulation = 0;
        }

        // TODO: should it be backward?
        DRAM.operate();
        LLC.operate();
    }

    uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time),
             elapsed_minute = elapsed_second / 60,
             elapsed_hour = elapsed_minute / 60;
    elapsed_minute -= elapsed_hour*60;
    elapsed_second -= (elapsed_hour*3600 + elapsed_minute*60);
    
    cout << endl << "ChampSim completed all CPUs" << endl;
    if (NUM_CPUS > 1) {
        cout << endl << "Total Simulation Statistics (not including warmup)" << endl;
        for (uint32_t i=0; i<NUM_CPUS; i++) {
            cout << endl << "CPU " << i << " cumulative IPC: " << (float) (ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr) / (current_core_cycle[i] - ooo_cpu[i].begin_sim_cycle); 
            cout << " instructions: " << ooo_cpu[i].num_retired - ooo_cpu[i].begin_sim_instr << " cycles: " << current_core_cycle[i] - ooo_cpu[i].begin_sim_cycle << endl;
#ifndef CRC2_COMPILE
            print_sim_stats(i, &ooo_cpu[i].L1D);
            print_sim_stats(i, &ooo_cpu[i].L1I);
            print_sim_stats(i, &ooo_cpu[i].L2C);
	    ooo_cpu[i].l1i_prefetcher_final_stats();
            ooo_cpu[i].L1D.l1d_prefetcher_final_stats();
	    ooo_cpu[i].L2C.l2c_prefetcher_final_stats();
#endif
            print_sim_stats(i, &LLC);
        }
        LLC.llc_prefetcher_final_stats();
    }

    cout << endl << "Region of Interest Statistics" << endl;
    for (uint32_t i=0; i<NUM_CPUS; i++) {
        cout << endl << "CPU " << i << " cumulative IPC: " << ((float) ooo_cpu[i].finish_sim_instr / ooo_cpu[i].finish_sim_cycle); 
        cout << " instructions: " << ooo_cpu[i].finish_sim_instr << " cycles: " << ooo_cpu[i].finish_sim_cycle << endl;
#ifndef CRC2_COMPILE
        print_roi_stats(i, &ooo_cpu[i].L1D);
        print_roi_stats(i, &ooo_cpu[i].L1I);
        print_roi_stats(i, &ooo_cpu[i].L2C);
#endif
        print_roi_stats(i, &LLC);
        //cout << "Major fault: " << major_fault[i] << " Minor fault: " << minor_fault[i] << endl;
    }

    for (uint32_t i=0; i<NUM_CPUS; i++) {
        ooo_cpu[i].l1i_prefetcher_final_stats();
        ooo_cpu[i].L1D.l1d_prefetcher_final_stats();
        ooo_cpu[i].L2C.l2c_prefetcher_final_stats();
    }

    LLC.llc_prefetcher_final_stats();

#ifndef CRC2_COMPILE
    LLC.llc_replacement_final_stats();
    print_dram_stats();
    print_branch_stats();
#endif

    return 0;
}
