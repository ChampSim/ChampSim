#ifndef CACHE_H
#define CACHE_H

#include <string>
#include <functional>
#include <list>
#include <vector>

#include "delay_queue.hpp"
#include "memory_class.h"

// CACHE TYPE
#define IS_ITLB 0
#define IS_DTLB 1
#define IS_STLB 2
#define IS_L1I  3
#define IS_L1D  4
#define IS_L2C  5
#define IS_LLC  6

// PAGE
extern uint32_t PAGE_TABLE_LATENCY, SWAP_LATENCY;

// virtual address space prefetching
#define VA_PREFETCH_TRANSLATION_LATENCY 2

class CACHE : public MemoryRequestConsumer, public MemoryRequestProducer {
  public:
    uint32_t cpu;
    const std::string NAME;
    const uint32_t NUM_SET, NUM_WAY, WQ_SIZE, RQ_SIZE, PQ_SIZE, MSHR_SIZE;
    const uint32_t HIT_LATENCY, FILL_LATENCY;
    std::vector<BLOCK> block{NUM_SET*NUM_WAY};
    int fill_level = -1;
    const uint32_t MAX_READ, MAX_WRITE;
    uint32_t reads_available_this_cycle, writes_available_this_cycle;
    uint8_t cache_type;

    // prefetch stats
    uint64_t pf_requested = 0,
             pf_issued = 0,
             pf_useful = 0,
             pf_useless = 0,
             pf_fill = 0;

    // queues
    champsim::delay_queue<PACKET> RQ{RQ_SIZE, HIT_LATENCY}, // read queue
                                  PQ{PQ_SIZE, HIT_LATENCY}, // prefetch queue
                                  VAPQ{PQ_SIZE, VA_PREFETCH_TRANSLATION_LATENCY}, // virtual address prefetch queue
                                  WQ{WQ_SIZE, HIT_LATENCY}; // write queue

    std::list<PACKET> MSHR{MSHR_SIZE}; // MSHR

    uint64_t sim_access[NUM_CPUS][NUM_TYPES] = {},
             sim_hit[NUM_CPUS][NUM_TYPES] = {},
             sim_miss[NUM_CPUS][NUM_TYPES] = {},
             roi_access[NUM_CPUS][NUM_TYPES] = {},
             roi_hit[NUM_CPUS][NUM_TYPES] = {},
             roi_miss[NUM_CPUS][NUM_TYPES] = {};

    uint64_t RQ_ACCESS = 0,
             RQ_MERGED = 0,
             RQ_FULL = 0,
             RQ_TO_CACHE = 0,
             PQ_ACCESS = 0,
             PQ_MERGED = 0,
             PQ_FULL = 0,
             PQ_TO_CACHE = 0,
             WQ_ACCESS = 0,
             WQ_MERGED = 0,
             WQ_FULL = 0,
             WQ_FORWARD = 0,
             WQ_TO_CACHE = 0;

    uint64_t total_miss_latency = 0;
    
    // constructor
    CACHE(std::string v1, uint32_t v2, int v3, uint32_t v5, uint32_t v6, uint32_t v7, uint32_t v8,
            uint32_t hit_lat, uint32_t fill_lat, uint32_t max_read, uint32_t max_write)
        : NAME(v1), NUM_SET(v2), NUM_WAY(v3), WQ_SIZE(v5), RQ_SIZE(v6), PQ_SIZE(v7), MSHR_SIZE(v8),
        HIT_LATENCY(hit_lat), FILL_LATENCY(fill_lat), MAX_READ(max_read), MAX_WRITE(max_write)
    {
    }

    // functions
    int  add_rq(PACKET *packet),
         add_wq(PACKET *packet),
         add_pq(PACKET *packet);

    void return_data(PACKET *packet),
         operate(),
         operate_writes(),
         operate_reads(),
         increment_WQ_FULL(uint64_t address);

    uint32_t get_occupancy(uint8_t queue_type, uint64_t address),
             get_size(uint8_t queue_type, uint64_t address);

    uint32_t get_set(uint64_t address),
             get_way(uint64_t address, uint32_t set);

    int  invalidate_entry(uint64_t inval_addr),
         prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr, int prefetch_fill_level, uint32_t prefetch_metadata),
         kpc_prefetch_line(uint64_t base_addr, uint64_t pf_addr, int prefetch_fill_level, int delta, int depth, int signature, int confidence, uint32_t prefetch_metadata),
         va_prefetch_line(uint64_t ip, uint64_t pf_addr, int prefetch_fill_level, uint32_t prefetch_metadata);

    void add_mshr(PACKET *packet),
         va_translate_prefetches();

    void handle_fill(),
         handle_writeback(),
         handle_read(),
         handle_prefetch();

    void readlike_hit(std::size_t set, std::size_t way, PACKET &handle_pkt);
    bool readlike_miss(PACKET &handle_pkt);
    bool filllike_miss(std::size_t set, std::size_t way, PACKET &handle_pkt);

    void prefetcher_operate    (uint64_t v_addr, uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type),
         (*l1i_prefetcher_cache_operate)(uint32_t, uint64_t, uint8_t, uint8_t),
         l1d_prefetcher_operate(uint64_t v_addr, uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type);

    uint32_t l2c_prefetcher_operate(uint64_t v_addr, uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in),
         llc_prefetcher_operate(uint64_t v_addr, uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in);

    void (*l1i_prefetcher_cache_fill)(uint32_t, uint64_t, uint32_t, uint32_t, uint8_t, uint64_t);
    void prefetcher_cache_fill(uint64_t v_addr, uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr),
             l1d_prefetcher_cache_fill(uint64_t v_addr, uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in);
    uint32_t l2c_prefetcher_cache_fill(uint64_t v_addr, uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in),
             llc_prefetcher_cache_fill(uint64_t v_addr, uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in);

    void prefetcher_initialize(),
         l1d_prefetcher_initialize(),
         l2c_prefetcher_initialize(),
         llc_prefetcher_initialize();

    void prefetcher_final_stats(),
         l1d_prefetcher_final_stats(),
         l2c_prefetcher_final_stats(),
         llc_prefetcher_final_stats();

    void llc_initialize_replacement();

    std::function<void()> replacement_final_stats;
    void llc_replacement_final_stats();

    std::function<void(uint32_t, uint32_t, uint32_t, uint64_t, uint64_t, uint64_t, uint32_t, uint8_t)> update_replacement_state;
    void llc_update_replacement_state(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit);

    std::function<uint32_t(uint32_t, uint64_t, uint32_t, const BLOCK*, uint64_t, uint64_t, uint32_t)> find_victim;
    uint32_t llc_find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK *current_set, uint64_t ip, uint64_t full_addr, uint32_t type);

    void lru_initialize();
    uint32_t lru_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK *current_set, uint64_t ip, uint64_t full_addr, uint32_t type);
    void lru_update(uint32_t set, uint32_t way, uint32_t type, uint8_t hit);
    void lru_final_stats();
};

#endif

