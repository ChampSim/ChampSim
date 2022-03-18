#ifndef CACHE_H
#define CACHE_H

#include <deque>
#include <functional>
#include <list>
#include <string>
#include <vector>

#include "champsim_constants.h"
#include "delay_queue.hpp"
#include "memory_class.h"
#include "operable.h"

class CACHE : public champsim::operable, public MemoryRequestConsumer, public MemoryRequestProducer
{
  enum FILL_LEVEL { FILL_L1 = 1, FILL_L2 = 2, FILL_LLC = 4, FILL_DRC = 8, FILL_DRAM = 16 };

  bool handle_fill(PACKET &fill_mshr);
  bool handle_writeback(PACKET &handle_pkt);
  bool handle_read(PACKET &handle_pkt);
  bool handle_prefetch(PACKET &handle_pkt);

  class BLOCK
  {
  public:
    bool valid = false;
    bool prefetch = false;
    bool dirty = false;

    uint64_t address = 0;
    uint64_t v_address = 0;
    uint64_t data = 0;
    uint64_t ip = 0;
    uint64_t cpu = 0;
    uint64_t instr_id = 0;

    uint32_t pf_metadata = 0;
  };

public:
  struct NonTranslatingQueues : public champsim::operable
  {
    std::deque<PACKET> RQ, PQ, WQ;
    const std::size_t RQ_SIZE, PQ_SIZE, WQ_SIZE;
    const uint64_t HIT_LATENCY;
    const std::size_t OFFSET_BITS;
    const bool match_offset_bits;

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

    NonTranslatingQueues(double freq_scale, std::size_t rq_size, std::size_t pq_size, std::size_t wq_size, uint64_t hit_latency, std::size_t offset_bits, bool match_offset) : champsim::operable(freq_scale), RQ_SIZE(rq_size), PQ_SIZE(pq_size), WQ_SIZE(wq_size), HIT_LATENCY(hit_latency), OFFSET_BITS(offset_bits), match_offset_bits(match_offset) {}
    void operate() override;

    bool add_rq(const PACKET &packet);
    bool add_wq(const PACKET &packet);
    bool add_pq(const PACKET &packet);

    virtual bool rq_has_ready() const;
    virtual bool wq_has_ready() const;
    virtual bool pq_has_ready() const;

    private:
    void check_collision();
  };

  struct TranslatingQueues : public NonTranslatingQueues, public MemoryRequestProducer
  {
    void operate() override;

    void issue_translation();
    void detect_misses();

    bool rq_has_ready() const override;
    bool wq_has_ready() const override;
    bool pq_has_ready() const override;

    void return_data(const PACKET &packet) override;

    using NonTranslatingQueues::NonTranslatingQueues;
  };

  uint32_t cpu;
  const std::string NAME;
  const uint32_t NUM_SET, NUM_WAY, MSHR_SIZE;
  const uint32_t FILL_LATENCY, OFFSET_BITS;
  std::vector<BLOCK> block{NUM_SET * NUM_WAY};
  const uint32_t MAX_READ, MAX_WRITE;
  const bool prefetch_as_load;
  const bool match_offset_bits;
  const bool virtual_prefetch;
  bool ever_seen_data = false;
  const unsigned pref_activate_mask = (1 << static_cast<int>(LOAD)) | (1 << static_cast<int>(PREFETCH));

  // prefetch stats
  uint64_t pf_requested = 0, pf_issued = 0, pf_useful = 0, pf_useless = 0, pf_fill = 0;

  NonTranslatingQueues &queues;
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

  uint32_t get_occupancy(uint8_t queue_type, uint64_t address) override;
  uint32_t get_size(uint8_t queue_type, uint64_t address) override;

  uint32_t get_set(uint64_t address);
  uint32_t get_way(uint64_t address, uint32_t set);

  int invalidate_entry(uint64_t inval_addr);
  int prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata);
  int prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata); // deprecated

  void readlike_hit(std::size_t set, std::size_t way, const PACKET& handle_pkt);
  bool readlike_miss(const PACKET& handle_pkt);
  bool filllike_miss(std::size_t set, std::size_t way, const PACKET& handle_pkt);

  bool should_activate_prefetcher(const PACKET &pkt) const;

  void print_deadlock() override;

#include "cache_modules.inc"

  const repl_t repl_type;
  const pref_t pref_type;

  // constructor
  CACHE(std::string v1, double freq_scale, uint32_t v2, int v3, uint32_t v8,
        uint32_t fill_lat, uint32_t max_read, uint32_t max_write, std::size_t offset_bits, bool pref_load, bool wq_full_addr, bool va_pref,
        unsigned pref_act_mask, NonTranslatingQueues &queues, MemoryRequestConsumer* ll, pref_t pref, repl_t repl)
      : champsim::operable(freq_scale), MemoryRequestProducer(ll), NAME(v1), NUM_SET(v2), NUM_WAY(v3),
        MSHR_SIZE(v8), FILL_LATENCY(fill_lat), OFFSET_BITS(offset_bits), MAX_READ(max_read),
        MAX_WRITE(max_write), prefetch_as_load(pref_load), match_offset_bits(wq_full_addr), virtual_prefetch(va_pref), pref_activate_mask(pref_act_mask),
        queues(queues), repl_type(repl), pref_type(pref)
  {
  }
};

#endif
