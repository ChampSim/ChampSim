#ifndef OOO_CPU_H
#define OOO_CPU_H

#include <array>
#include <queue>
#include <functional>

#include "champsim_constants.h"
#include "delay_queue.hpp"
#include "instruction.h"
#include "cache.h"
#include "instruction.h"

#define DEADLOCK_CYCLE 1000000

using namespace std;

class CacheBus : public MemoryRequestProducer
{
    public:
        champsim::circular_buffer<PACKET> PROCESSED;
        CacheBus(std::size_t q_size, MemoryRequestConsumer *ll) : MemoryRequestProducer(ll), PROCESSED(q_size) {}
        void return_data(PACKET *packet);
};

// cpu
class O3_CPU {
  public:
    uint32_t cpu = 0;

    // instruction
    uint64_t instr_unique_id = 0, completed_executions = 0,
             begin_sim_cycle = 0, begin_sim_instr,
             last_sim_cycle = 0, last_sim_instr = 0,
             finish_sim_cycle = 0, finish_sim_instr = 0,
             instrs_to_read_this_cycle = 0, instrs_to_fetch_this_cycle = 0,
             next_print_instruction = STAT_PRINTING_PERIOD, num_retired = 0;
    uint32_t inflight_reg_executions = 0, inflight_mem_executions = 0, num_searched = 0;

    struct dib_entry_t
    {
        bool valid = false;
        unsigned lru = 999999;
        uint64_t address = 0;
    };

    // instruction buffer
    using dib_t= std::vector<dib_entry_t>;
    const std::size_t dib_set, dib_way, dib_window;
    dib_t DIB{dib_set*dib_way};

    // reorder buffer, load/store queue, register file
    champsim::circular_buffer<ooo_model_instr> IFETCH_BUFFER;
    champsim::delay_queue<ooo_model_instr> DISPATCH_BUFFER;
    champsim::delay_queue<ooo_model_instr> DECODE_BUFFER;
    CORE_BUFFER<ooo_model_instr> ROB;
    CORE_BUFFER<LSQ_ENTRY> LQ, SQ;

    // Constants
    const unsigned FETCH_WIDTH, DECODE_WIDTH, DISPATCH_WIDTH, SCHEDULER_SIZE, EXEC_WIDTH, LQ_WIDTH, SQ_WIDTH, RETIRE_WIDTH;
    const unsigned BRANCH_MISPREDICT_PENALTY, SCHEDULING_LATENCY, EXEC_LATENCY;

    // store array, this structure is required to properly handle store instructions
    std::queue<uint64_t> STA;

    // Ready-To-Execute
    std::queue<uint32_t> ready_to_execute;

    // Ready-To-Load
    std::queue<uint32_t> RTL0, RTL1;

    // Ready-To-Store
    std::queue<uint32_t> RTS0, RTS1;

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
    CACHE ITLB{"ITLB", ITLB_SET, ITLB_WAY, ITLB_WQ_SIZE, ITLB_RQ_SIZE, ITLB_PQ_SIZE, ITLB_MSHR_SIZE, ITLB_MAX_READ, ITLB_MAX_WRITE, ITLB_LATENCY},
          DTLB{"DTLB", DTLB_SET, DTLB_WAY, DTLB_WQ_SIZE, DTLB_RQ_SIZE, DTLB_PQ_SIZE, DTLB_MSHR_SIZE, DTLB_MAX_READ, DTLB_MAX_WRITE, DTLB_LATENCY},
          STLB{"STLB", STLB_SET, STLB_WAY, STLB_WQ_SIZE, STLB_RQ_SIZE, STLB_PQ_SIZE, STLB_MSHR_SIZE, STLB_MAX_READ, STLB_MAX_WRITE, STLB_LATENCY},
          L1I{"L1I", L1I_SET, L1I_WAY, L1I_WQ_SIZE, L1I_RQ_SIZE, L1I_PQ_SIZE, L1I_MSHR_SIZE, L1I_MAX_READ, L1I_MAX_WRITE, L1I_LATENCY},
          L1D{"L1D", L1D_SET, L1D_WAY, L1D_WQ_SIZE, L1D_RQ_SIZE, L1D_PQ_SIZE, L1D_MSHR_SIZE, L1D_MAX_READ, L1D_MAX_WRITE, L1D_LATENCY},
          L2C{"L2C", L2C_SET, L2C_WAY, L2C_WQ_SIZE, L2C_RQ_SIZE, L2C_PQ_SIZE, L2C_MSHR_SIZE, L2C_MAX_READ, L2C_MAX_WRITE, L2C_LATENCY};

    CacheBus ITLB_bus{ROB.SIZE, &ITLB}, DTLB_bus{ROB.SIZE, &DTLB}, L1I_bus{ROB.SIZE, &L1I}, L1D_bus{ROB.SIZE, &L1D};
  
    // constructor
    O3_CPU(uint32_t cpu, std::size_t dib_set, std::size_t dib_way, std::size_t dib_window,
            std::size_t ifetch_buffer_size, std::size_t decode_buffer_size, std::size_t dispatch_buffer_size,
            std::size_t rob_size, std::size_t lq_size, std::size_t sq_size,
            unsigned fetch_width, unsigned decode_width, unsigned dispatch_width, unsigned schedule_width,
            unsigned execute_width, unsigned lq_width, unsigned sq_width, unsigned retire_width,
            unsigned mispredict_penalty, unsigned decode_latency, unsigned dispatch_latency, unsigned schedule_latency, unsigned execute_latency) :
        cpu(cpu), dib_set(dib_set), dib_way(dib_way), dib_window(dib_window),
        IFETCH_BUFFER(ifetch_buffer_size), DISPATCH_BUFFER(dispatch_buffer_size, dispatch_latency), DECODE_BUFFER(decode_buffer_size, decode_latency),
        ROB("ROB", rob_size), LQ("LQ", lq_size), SQ("SQ", sq_size),
        FETCH_WIDTH(fetch_width), DECODE_WIDTH(decode_width), DISPATCH_WIDTH(dispatch_width), SCHEDULER_SIZE(schedule_width),
        EXEC_WIDTH(execute_width), LQ_WIDTH(lq_width), SQ_WIDTH(sq_width), RETIRE_WIDTH(retire_width),
        BRANCH_MISPREDICT_PENALTY(mispredict_penalty), SCHEDULING_LATENCY(schedule_latency), EXEC_LATENCY(execute_latency)
    {
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
    void fetch_instruction(),
         decode_instruction(),
         dispatch_instruction(),
         schedule_instruction(),
         execute_instruction(),
         schedule_memory_instruction(),
         execute_memory_instruction(),
         do_scheduling(uint32_t rob_index),  
         reg_dependency(uint32_t rob_index),
         do_execution(uint32_t rob_index),
         do_memory_scheduling(uint32_t rob_index),
         operate_lsq();
    uint32_t complete_execution(uint32_t rob_index);
    void reg_RAW_dependency(uint32_t prior, uint32_t current, uint32_t source_index),
         reg_RAW_release(uint32_t rob_index),
         mem_RAW_dependency(uint32_t prior, uint32_t current, uint32_t data_index, uint32_t lq_index),
         release_load_queue(uint32_t lq_index);

    void initialize_core();
    void add_load_queue(uint32_t rob_index, uint32_t data_index),
         add_store_queue(uint32_t rob_index, uint32_t data_index),
         execute_store(uint32_t rob_index, uint32_t sq_index, uint32_t data_index);
    int  execute_load(uint32_t rob_index, uint32_t sq_index, uint32_t data_index);
    void check_dependency(int prior, int current);
    void operate_cache();
    void complete_inflight_instruction();
    void handle_memory_return();
    void retire_rob();

    uint32_t check_rob(uint64_t instr_id);

    uint32_t check_and_add_lsq(uint32_t rob_index);

    uint8_t mem_reg_dependence_resolved(uint32_t rob_index);

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

