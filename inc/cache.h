#ifndef CACHE_H
#define CACHE_H

#include <functional>
#include <list>
#include <string>
#include <vector>

#include "champsim.h"
#include "delay_queue.hpp"
#include "memory_class.h"
#include "ooo_cpu.h"
#include "operable.h"

// virtual address space prefetching
#define VA_PREFETCH_TRANSLATION_LATENCY 2

extern std::array<O3_CPU*, NUM_CPUS> ooo_cpu;

class CACHE : public champsim::operable, public MemoryRequestConsumer, public MemoryRequestProducer
{
  // queues
  struct TranslatingQueues
  {
    std::deque<PACKET> RQ, PQ, VAPQ, WQ;
    const std::size_t RQ_SIZE, PQ_SIZE, WQ_SIZE;

    uint64_t RQ_ACCESS = 0;
    uint64_t RQ_MERGED = 0;
    uint64_t RQ_FULL = 0;
    uint64_t RQ_TO_CACHE = 0;
    uint64_t PQ_ACCESS = 0;
    uint64_t PQ_MERGED = 0;
    uint64_t PQ_FULL = 0;
    uint64_t PQ_TO_CACHE = 0;
    uint64_t WQ_ACCESS = 0;
    uint64_t WQ_MERGED = 0;
    uint64_t WQ_FULL = 0;
    uint64_t WQ_FORWARD = 0;
    uint64_t WQ_TO_CACHE = 0;

    TranslatingQueues(std::size_t rq_size, std::size_t pq_size, std::size_t wq_size) : RQ_SIZE(rq_size), PQ_SIZE(pq_size), WQ_SIZE(wq_size) {}
    void check_collision(std::size_t write_shamt, std::size_t read_shamt);

    // functions
    bool add_rq(const PACKET &packet);
    bool add_wq(const PACKET &packet);
    bool add_pq(const PACKET &packet);
  };

public:
  uint32_t cpu;
  const std::string NAME;
  const uint32_t NUM_SET, NUM_WAY, MSHR_SIZE;
  const uint32_t HIT_LATENCY, FILL_LATENCY, OFFSET_BITS;
  std::vector<BLOCK> block{NUM_SET * NUM_WAY};
  const uint32_t MAX_READ, MAX_WRITE;
  uint32_t reads_available_this_cycle, writes_available_this_cycle;
  const bool prefetch_as_load;
  const bool match_offset_bits;
  const bool virtual_prefetch;
  bool ever_seen_data = false;
  const unsigned pref_activate_mask = (1 << static_cast<int>(LOAD)) | (1 << static_cast<int>(PREFETCH));

  // prefetch stats
  uint64_t pf_requested = 0, pf_issued = 0, pf_useful = 0, pf_useless = 0, pf_fill = 0;

  TranslatingQueues queues;
  std::list<PACKET> MSHR;

  uint64_t sim_access[NUM_CPUS][NUM_TYPES] = {}, sim_hit[NUM_CPUS][NUM_TYPES] = {}, sim_miss[NUM_CPUS][NUM_TYPES] = {}, roi_access[NUM_CPUS][NUM_TYPES] = {},
           roi_hit[NUM_CPUS][NUM_TYPES] = {}, roi_miss[NUM_CPUS][NUM_TYPES] = {};

  uint64_t total_miss_latency = 0;

  // functions
  bool add_rq(const PACKET &packet) override;
  bool add_wq(const PACKET &packet) override;
  bool add_pq(const PACKET &packet) override;

  void return_data(const PACKET &packet) override;
  void operate() override;
  void operate_writes();
  void operate_reads();

  uint32_t get_occupancy(uint8_t queue_type, uint64_t address) override;
  uint32_t get_size(uint8_t queue_type, uint64_t address) override;

  uint32_t get_set(uint64_t address);
  uint32_t get_way(uint64_t address, uint32_t set);

  int invalidate_entry(uint64_t inval_addr);
  int prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata);
  int prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata); // deprecated

  void add_mshr(PACKET* packet);
  void va_translate_prefetches();

  void handle_fill();
  void handle_writeback();
  void handle_read();
  void handle_prefetch();

  void readlike_hit(std::size_t set, std::size_t way, const PACKET& handle_pkt);
  bool readlike_miss(const PACKET& handle_pkt);
  bool filllike_miss(std::size_t set, std::size_t way, const PACKET& handle_pkt);

  bool should_activate_prefetcher(int type);

  void print_deadlock() override;

#include "cache_modules.inc"

  const repl_t repl_type;
  const pref_t pref_type;

  // constructor
  CACHE(std::string v1, double freq_scale, unsigned fill_level, uint32_t v2, int v3, uint32_t v5, uint32_t v6, uint32_t v7, uint32_t v8, uint32_t hit_lat,
        uint32_t fill_lat, uint32_t max_read, uint32_t max_write, std::size_t offset_bits, bool pref_load, bool wq_full_addr, bool va_pref,
        unsigned pref_act_mask, MemoryRequestConsumer* ll, pref_t pref, repl_t repl)
      : champsim::operable(freq_scale), MemoryRequestConsumer(fill_level), MemoryRequestProducer(ll), NAME(v1), NUM_SET(v2), NUM_WAY(v3),
        MSHR_SIZE(v8), HIT_LATENCY(hit_lat), FILL_LATENCY(fill_lat), OFFSET_BITS(offset_bits), MAX_READ(max_read),
        MAX_WRITE(max_write), prefetch_as_load(pref_load), match_offset_bits(wq_full_addr), virtual_prefetch(va_pref), pref_activate_mask(pref_act_mask),
        queues(v6, v7, v5), repl_type(repl), pref_type(pref)
  {
  }
};

#endif
