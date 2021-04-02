#ifndef CACHE_H
#define CACHE_H

#include <string>
#include <functional>
#include <list>
#include <vector>

#include "delay_queue.hpp"
#include "memory_class.h"
#include "operable.h"

// CACHE TYPE
#define IS_ITLB 0
#define IS_DTLB 1
#define IS_STLB 2
#define IS_L1I  3
#define IS_L1D  4
#define IS_L2C  5
#define IS_LLC  6

#define IS_PTW 7

// PAGE
extern uint32_t PAGE_TABLE_LATENCY, SWAP_LATENCY;

// virtual address space prefetching
#define VA_PREFETCH_TRANSLATION_LATENCY 2

class CACHE : public champsim::operable, public MemoryRequestConsumer, public MemoryRequestProducer {
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
    const bool prefetch_as_load;
    const bool virtual_prefetch;

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

    std::list<PACKET> MSHR; // MSHR

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
  CACHE(std::string v1, double freq_scale, uint32_t v2, int v3, uint32_t v5, uint32_t v6, uint32_t v7, uint32_t v8,
            uint32_t hit_lat, uint32_t fill_lat, uint32_t max_read, uint32_t max_write, bool pref_load, bool va_pref, MemoryRequestConsumer *ll,
            std::function<void(CACHE*)> pref_init,
            std::function<uint32_t(CACHE*, uint64_t, uint64_t, uint8_t, uint8_t, uint32_t)> pref_operate,
            std::function<uint32_t(CACHE*, uint64_t, uint32_t, uint32_t, uint8_t, uint64_t, uint32_t)> pref_cache_fill,
            std::function<void(CACHE*)> pref_cycle,
            std::function<void(CACHE*)> pref_final_stats,
            std::function<void(CACHE*)> repl_init,
            std::function<uint32_t(CACHE*, uint32_t, uint64_t, uint32_t, const BLOCK*, uint64_t, uint64_t, uint32_t)> repl_find_victim,
            std::function<void(CACHE*, uint32_t, uint32_t, uint32_t, uint64_t, uint64_t, uint64_t, uint32_t, uint8_t)> repl_update_replacement_state,
            std::function<void(CACHE*)> repl_final_stats )
        : champsim::operable(freq_scale), MemoryRequestProducer(ll), NAME(v1), NUM_SET(v2), NUM_WAY(v3), WQ_SIZE(v5), RQ_SIZE(v6), PQ_SIZE(v7), MSHR_SIZE(v8),
        HIT_LATENCY(hit_lat), FILL_LATENCY(fill_lat), MAX_READ(max_read), MAX_WRITE(max_write), prefetch_as_load(pref_load), virtual_prefetch(va_pref),
        impl_prefetcher_initialize(std::bind(pref_init, this)),
        impl_prefetcher_operate(std::bind(pref_operate, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5)),
        impl_prefetcher_cache_fill(std::bind(pref_cache_fill, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6)),
        impl_prefetcher_cycle_operate(std::bind(pref_cycle, this)),
        impl_prefetcher_final_stats(std::bind(pref_final_stats, this)),
        impl_replacement_initialize(std::bind(repl_init, this)),
        impl_replacement_final_stats(std::bind(repl_final_stats, this)),
        impl_update_replacement_state(std::bind(repl_update_replacement_state, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7, std::placeholders::_8)),
        impl_find_victim(std::bind(repl_find_victim, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7))
    {
    }

    // functions
    int  add_rq(PACKET *packet),
         add_wq(PACKET *packet),
         add_pq(PACKET *packet);

    void return_data(PACKET *packet),
         operate(),
         operate_writes(),
         operate_reads();

    uint32_t get_occupancy(uint8_t queue_type, uint64_t address),
             get_size(uint8_t queue_type, uint64_t address);

    uint32_t get_set(uint64_t address),
             get_way(uint64_t address, uint32_t set);

    int  invalidate_entry(uint64_t inval_addr),
         prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr, int prefetch_fill_level, uint32_t prefetch_metadata),
         kpc_prefetch_line(uint64_t base_addr, uint64_t pf_addr, int prefetch_fill_level, int delta, int depth, int signature, int confidence, uint32_t prefetch_metadata);

    void add_mshr(PACKET *packet),
         va_translate_prefetches();

    void handle_fill(),
         handle_writeback(),
         handle_read(),
         handle_prefetch();

    void readlike_hit(std::size_t set, std::size_t way, PACKET &handle_pkt);
    bool readlike_miss(PACKET &handle_pkt);
    bool filllike_miss(std::size_t set, std::size_t way, PACKET &handle_pkt);

    void cpu_redir_ipref_initialize();
    uint32_t cpu_redir_ipref_operate(uint64_t, uint64_t, uint8_t, uint8_t, uint32_t);
    uint32_t cpu_redir_ipref_fill(uint64_t, uint32_t, uint32_t, uint8_t, uint64_t, uint32_t);
    void cpu_redir_ipref_cycle_operate();
    void cpu_redir_ipref_final_stats();

    const std::function<void()> impl_prefetcher_initialize;
    const std::function<uint32_t(uint64_t, uint64_t, uint8_t, uint8_t, uint32_t)> impl_prefetcher_operate;
    const std::function<uint32_t(uint64_t, uint32_t, uint32_t, uint8_t, uint64_t, uint32_t)> impl_prefetcher_cache_fill;
    const std::function<void()> impl_prefetcher_cycle_operate;
    const std::function<void()> impl_prefetcher_final_stats;

    const std::function<void()> impl_replacement_initialize;
    const std::function<void()> impl_replacement_final_stats;
    const std::function<void(uint32_t, uint32_t, uint32_t, uint64_t, uint64_t, uint64_t, uint32_t, uint8_t)> impl_update_replacement_state;
    const std::function<uint32_t(uint32_t, uint64_t, uint32_t, const BLOCK*, uint64_t, uint64_t, uint32_t)> impl_find_victim;

#include "cache_modules.inc"
};

template <typename T>
struct eq_full_addr
{
    using argument_type = T;
    const decltype(argument_type::address) val;
    eq_full_addr(decltype(argument_type::address) val) : val(val) {}
    bool operator()(const argument_type &test)
    {
        is_valid<argument_type> validtest;
        return validtest(test) && test.full_addr == val;
    }
};

#endif

