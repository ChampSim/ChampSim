#ifndef OOO_CPU_H
#define OOO_CPU_H

#include <array>
#include <functional>
#include <queue>

#include "champsim_constants.h"
#include "delay_queue.hpp"
#include "instruction.h"
#include "cache.h"
#include "instruction.h"
#include "ptw.h"

#define DEADLOCK_CYCLE 1000000

using namespace std;

#define STA_SIZE (ROB_SIZE*NUM_INSTR_DESTINATIONS_SPARC)

class CacheBus : public MemoryRequestProducer
{
    public:
        champsim::circular_buffer<PACKET> PROCESSED{ROB_SIZE};
        explicit CacheBus(MemoryRequestConsumer *ll) : MemoryRequestProducer(ll) {}
        void return_data(PACKET *packet);
};

// cpu
class O3_CPU {
  public:
    uint32_t cpu = 0;

    // trace
    FILE *trace_file = NULL;
    char trace_string[1024];
    char gunzip_command[1024];

    // instruction
    input_instr current_instr;
    cloudsuite_instr current_cloudsuite_instr;
    uint64_t instr_unique_id = 0, completed_executions = 0,
             begin_sim_cycle = 0, begin_sim_instr,
             last_sim_cycle = 0, last_sim_instr = 0,
             finish_sim_cycle = 0, finish_sim_instr = 0,
             warmup_instructions, simulation_instructions, instrs_to_read_this_cycle = 0, instrs_to_fetch_this_cycle = 0,
             next_print_instruction = STAT_PRINTING_PERIOD, num_retired = 0;
    uint32_t inflight_reg_executions = 0, inflight_mem_executions = 0;
    uint32_t next_ITLB_fetch = 0;

    struct dib_entry_t
    {
        bool valid = false;
        unsigned lru = DIB_WAY;
        uint64_t address = 0;
    };

    // instruction buffer
    using dib_t= std::array<std::array<dib_entry_t, DIB_WAY>, DIB_SET>;
    dib_t DIB;

    // reorder buffer, load/store queue, register file
    champsim::circular_buffer<ooo_model_instr> IFETCH_BUFFER{IFETCH_BUFFER_SIZE};
    champsim::delay_queue<ooo_model_instr> DISPATCH_BUFFER{DISPATCH_BUFFER_SIZE, DISPATCH_LATENCY};
    champsim::delay_queue<ooo_model_instr> DECODE_BUFFER{DECODE_BUFFER_SIZE, DECODE_LATENCY};
    champsim::circular_buffer<ooo_model_instr> ROB{ROB_SIZE};
    std::vector<LSQ_ENTRY> LQ{LQ_SIZE};
    std::vector<LSQ_ENTRY> SQ{SQ_SIZE};

    // store array, this structure is required to properly handle store instructions
    uint64_t STA[STA_SIZE], STA_head = 0, STA_tail = 0;

    // Ready-To-Execute
    std::queue<champsim::circular_buffer<ooo_model_instr>::iterator> ready_to_execute;

    // Ready-To-Load
    std::queue<std::vector<LSQ_ENTRY>::iterator> RTL0, RTL1;

    // Ready-To-Store
    std::queue<std::vector<LSQ_ENTRY>::iterator> RTS0, RTS1;

    // branch
    int branch_mispredict_stall_fetch = 0; // flag that says that we should stall because a branch prediction was wrong
    int mispredicted_branch_iw_index = 0; // index in the instruction window of the mispredicted branch.  fetch resumes after the instruction at this index executes
    uint8_t  fetch_stall = 0;
    uint64_t fetch_resume_cycle = 0;
    uint64_t num_branch = 0, branch_mispredictions = 0;
    uint64_t total_rob_occupancy_at_branch_mispredict;

    uint64_t total_branch_types[8] = {};
    uint64_t branch_type_misses[8] = {};

    // TLBs and caches
    CACHE ITLB{"ITLB", ITLB_SET, ITLB_WAY, ITLB_WQ_SIZE, ITLB_RQ_SIZE, ITLB_PQ_SIZE, ITLB_MSHR_SIZE, ITLB_HIT_LATENCY, ITLB_FILL_LATENCY, ITLB_MAX_READ, ITLB_MAX_WRITE, ITLB_PREF_LOAD, false},
          DTLB{"DTLB", DTLB_SET, DTLB_WAY, DTLB_WQ_SIZE, DTLB_RQ_SIZE, DTLB_PQ_SIZE, DTLB_MSHR_SIZE, DTLB_HIT_LATENCY, DTLB_FILL_LATENCY, DTLB_MAX_READ, DTLB_MAX_WRITE, DTLB_PREF_LOAD, false},
          STLB{"STLB", STLB_SET, STLB_WAY, STLB_WQ_SIZE, STLB_RQ_SIZE, STLB_PQ_SIZE, STLB_MSHR_SIZE, STLB_HIT_LATENCY, STLB_FILL_LATENCY, STLB_MAX_READ, STLB_MAX_WRITE, STLB_PREF_LOAD, false},
          L1I{"L1I", L1I_SET, L1I_WAY, L1I_WQ_SIZE, L1I_RQ_SIZE, L1I_PQ_SIZE, L1I_MSHR_SIZE, L1I_HIT_LATENCY, L1I_FILL_LATENCY, L1I_MAX_READ, L1I_MAX_WRITE, L1I_PREF_LOAD, true},
          L1D{"L1D", L1D_SET, L1D_WAY, L1D_WQ_SIZE, L1D_RQ_SIZE, L1D_PQ_SIZE, L1D_MSHR_SIZE, L1D_HIT_LATENCY, L1D_FILL_LATENCY, L1D_MAX_READ, L1D_MAX_WRITE, L1D_PREF_LOAD, L1D_VA_PREF},
          L2C{"L2C", L2C_SET, L2C_WAY, L2C_WQ_SIZE, L2C_RQ_SIZE, L2C_PQ_SIZE, L2C_MSHR_SIZE, L2C_HIT_LATENCY, L2C_FILL_LATENCY, L2C_MAX_READ, L2C_MAX_WRITE, L2C_PREF_LOAD, L2C_VA_PREF};

    CacheBus ITLB_bus{&ITLB}, DTLB_bus{&DTLB}, L1I_bus{&L1I}, L1D_bus{&L1D};
  
	PageTableWalker PTW{"PTW", PSCL5_SET, PSCL5_WAY, PSCL4_SET, PSCL4_WAY, PSCL3_SET, PSCL3_WAY, PSCL2_SET, PSCL2_WAY, PTW_RQ_SIZE, PTW_MSHR_SIZE, PTW_MAX_READ, PTW_MAX_WRITE};

    // constructor
    O3_CPU(uint32_t cpu, uint64_t warmup_instructions, uint64_t simulation_instructions) : cpu(cpu), begin_sim_cycle(warmup_instructions), warmup_instructions(warmup_instructions), simulation_instructions(simulation_instructions)
    {
        for (uint32_t i=0; i<STA_SIZE; i++)
	  STA[i] = UINT64_MAX;

        // BRANCH PREDICTOR & BTB
        initialize_branch_predictor();
	initialize_btb();

        // TLBs
        ITLB.cpu = this->cpu;
        ITLB.cache_type = IS_ITLB;
        ITLB.fill_level = FILL_L1;
        ITLB.lower_level = &STLB;

        DTLB.cpu = this->cpu;
        DTLB.cache_type = IS_DTLB;
        DTLB.fill_level = FILL_L1;
        DTLB.lower_level = &STLB;

        STLB.cpu = this->cpu;
        STLB.cache_type = IS_STLB;
        STLB.fill_level = FILL_L2;
		STLB.lower_level = &PTW;

		PTW.cpu = this->cpu;
		PTW.cache_type = IS_PTW;
		PTW.lower_level = &L1D; //PTW checks L1 cache for cached translation blocks.

        // PRIVATE CACHE
        L1I.cpu = this->cpu;
        L1I.cache_type = IS_L1I;
        L1I.fill_level = FILL_L1;
        L1I.lower_level = &L2C;

        L1D.cpu = this->cpu;
        L1D.cache_type = IS_L1D;
        L1D.fill_level = FILL_L1;
        L1D.lower_level = &L2C;

        L2C.cpu = this->cpu;
        L2C.cache_type = IS_L2C;
        L2C.fill_level = FILL_L2;

        l1i_prefetcher_initialize();
        L1D.l1d_prefetcher_initialize();
        L2C.l2c_prefetcher_initialize();

        using namespace std::placeholders;

        ITLB.find_victim = std::bind(&CACHE::lru_victim, &ITLB, _1, _2, _3, _4, _5, _6, _7);
        DTLB.find_victim = std::bind(&CACHE::lru_victim, &DTLB, _1, _2, _3, _4, _5, _6, _7);
        STLB.find_victim = std::bind(&CACHE::lru_victim, &STLB, _1, _2, _3, _4, _5, _6, _7);
        L1I.find_victim = std::bind(&CACHE::lru_victim, &L1I, _1, _2, _3, _4, _5, _6, _7);
        L1D.find_victim = std::bind(&CACHE::lru_victim, &L1D, _1, _2, _3, _4, _5, _6, _7);
        L2C.find_victim = std::bind(&CACHE::lru_victim, &L2C, _1, _2, _3, _4, _5, _6, _7);

        ITLB.update_replacement_state = std::bind(&CACHE::lru_update, &ITLB, _2, _3, _7, _8);
        DTLB.update_replacement_state = std::bind(&CACHE::lru_update, &DTLB, _2, _3, _7, _8);
        STLB.update_replacement_state = std::bind(&CACHE::lru_update, &STLB, _2, _3, _7, _8);
        L1I.update_replacement_state = std::bind(&CACHE::lru_update, &L1I, _2, _3, _7, _8);
        L1D.update_replacement_state = std::bind(&CACHE::lru_update, &L1D, _2, _3, _7, _8);
        L2C.update_replacement_state = std::bind(&CACHE::lru_update, &L2C, _2, _3, _7, _8);

        ITLB.replacement_final_stats = std::bind(&CACHE::lru_final_stats, &ITLB);
        DTLB.replacement_final_stats = std::bind(&CACHE::lru_final_stats, &DTLB);
        STLB.replacement_final_stats = std::bind(&CACHE::lru_final_stats, &STLB);
        L1I.replacement_final_stats = std::bind(&CACHE::lru_final_stats, &L1I);
        L1D.replacement_final_stats = std::bind(&CACHE::lru_final_stats, &L1D);
        L2C.replacement_final_stats = std::bind(&CACHE::lru_final_stats, &L2C);
   }

    // functions
    uint32_t init_instruction(ooo_model_instr instr);
    void check_dib(),
         translate_fetch(),
         fetch_instruction(),
         promote_to_decode(),
         decode_instruction(),
         dispatch_instruction(),
         schedule_instruction(),
         execute_instruction(),
         schedule_memory_instruction(),
         execute_memory_instruction(),
         do_check_dib(ooo_model_instr &instr),
         do_translate_fetch(champsim::circular_buffer<ooo_model_instr>::iterator begin, champsim::circular_buffer<ooo_model_instr>::iterator end),
         do_fetch_instruction(champsim::circular_buffer<ooo_model_instr>::iterator begin, champsim::circular_buffer<ooo_model_instr>::iterator end),
         do_dib_update(const ooo_model_instr &instr),
         do_scheduling(champsim::circular_buffer<ooo_model_instr>::iterator rob_it),
         do_execution(champsim::circular_buffer<ooo_model_instr>::iterator rob_it),
         do_memory_scheduling(champsim::circular_buffer<ooo_model_instr>::iterator rob_it),
         operate_lsq(),
         do_complete_execution(champsim::circular_buffer<ooo_model_instr>::iterator rob_it),
         do_sq_forward_to_lq(LSQ_ENTRY &sq_entry, LSQ_ENTRY &lq_entry);

    void initialize_core();
    void add_load_queue(champsim::circular_buffer<ooo_model_instr>::iterator rob_index, uint32_t data_index),
         add_store_queue(champsim::circular_buffer<ooo_model_instr>::iterator rob_index, uint32_t data_index);
    void execute_store(std::vector<LSQ_ENTRY>::iterator sq_it);
    int  execute_load(std::vector<LSQ_ENTRY>::iterator lq_it);
    int  do_translate_store(std::vector<LSQ_ENTRY>::iterator sq_it);
    int  do_translate_load(std::vector<LSQ_ENTRY>::iterator sq_it);
    void check_dependency(int prior, int current);
    void operate_cache();
    void complete_inflight_instruction();
    void handle_memory_return();
    void retire_rob();

  // branch predictor
  uint8_t predict_branch(uint64_t ip, uint64_t predicted_target, uint8_t always_taken, uint8_t branch_type);
  void initialize_branch_predictor(),
    last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type);

  // btb
  std::pair<uint64_t, uint8_t> btb_prediction(uint64_t ip, uint8_t branch_type);
  void initialize_btb(),
    update_btb(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type);

  // code prefetching
  void l1i_prefetcher_initialize();
  void l1i_prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target);
  void l1i_prefetcher_cache_operate(uint64_t v_addr, uint8_t cache_hit, uint8_t prefetch_hit);
  void l1i_prefetcher_cycle_operate();
  void l1i_prefetcher_cache_fill(uint64_t v_addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_v_addr);
  void l1i_prefetcher_final_stats();
  int prefetch_code_line(uint64_t pf_v_addr);
};

#endif

