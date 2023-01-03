#ifndef CACHE_H
#define CACHE_H

#include <array>
#include <bitset>
#include <cassert>
#include <deque>
#include <functional>
#include <list>
#include <string>
#include <vector>

#include "champsim.h"
#include "champsim_constants.h"
#include "channel.h"
#include "memory_class.h"
#include "operable.h"

struct cache_stats {
  std::string name;
  // prefetch stats
  uint64_t pf_requested = 0;
  uint64_t pf_issued = 0;
  uint64_t pf_useful = 0;
  uint64_t pf_useless = 0;
  uint64_t pf_fill = 0;

  std::array<std::array<uint64_t, NUM_CPUS>, NUM_TYPES> hits = {};
  std::array<std::array<uint64_t, NUM_CPUS>, NUM_TYPES> misses = {};

  uint64_t total_miss_latency = 0;
};

class CACHE : public champsim::operable
{
  enum [[deprecated(
      "Prefetchers may not specify arbitrary fill levels. Use CACHE::prefetch_line(pf_addr, fill_this_level, prefetch_metadata) instead.")]] FILL_LEVEL{
      FILL_L1 = 1, FILL_L2 = 2, FILL_LLC = 4, FILL_DRC = 8, FILL_DRAM = 16};

  bool try_hit(const PACKET& handle_pkt);
  bool handle_fill(const PACKET& fill_mshr);
  bool handle_miss(const PACKET& handle_pkt);
  bool handle_write(const PACKET& handle_pkt);
  void finish_packet(const PACKET& packet);
  void finish_translation(const PACKET& packet);

  void issue_translation();
  void detect_misses();

  struct BLOCK {
    bool valid = false;
    bool prefetch = false;
    bool dirty = false;

    uint64_t address = 0;
    uint64_t v_address = 0;
    uint64_t data = 0;

    uint32_t pf_metadata = 0;
  };
  using set_type = std::vector<BLOCK>;

  std::pair<set_type::iterator, set_type::iterator> get_set_span(uint64_t address);
  std::pair<set_type::const_iterator, set_type::const_iterator> get_set_span(uint64_t address) const;
  std::size_t get_set_index(uint64_t address) const;

  std::deque<PACKET> internal_PQ{};
  std::deque<PACKET> inflight_tag_check{};
  std::deque<PACKET> translation_stash{};
  std::deque<PACKET> returned_data{};
  std::deque<PACKET> returned_translation{};

public:
  std::vector<champsim::channel*> upper_levels;
  champsim::channel* lower_level;
  champsim::channel* lower_translate;

  uint32_t cpu = 0;
  const std::string NAME;
  const uint32_t NUM_SET, NUM_WAY, MSHR_SIZE;
  const std::size_t PQ_SIZE;
  const uint64_t HIT_LATENCY, FILL_LATENCY;
  const unsigned OFFSET_BITS;
  set_type block{NUM_SET * NUM_WAY};
  const long int MAX_TAG, MAX_FILL;
  const bool prefetch_as_load;
  const bool match_offset_bits;
  const bool virtual_prefetch;
  bool ever_seen_data = false;
  const unsigned pref_activate_mask = (1 << static_cast<int>(LOAD)) | (1 << static_cast<int>(PREFETCH));

  using stats_type = cache_stats;

  std::vector<stats_type> sim_stats{}, roi_stats{};

  std::deque<PACKET> MSHR;
  std::deque<PACKET> inflight_writes;

  void operate() override final;

  void initialize() override final;
  void begin_phase() override final;
  void end_phase(unsigned cpu) override final;

  std::size_t get_occupancy(uint8_t queue_type, uint64_t address);
  std::size_t get_size(uint8_t queue_type, uint64_t address);

  [[deprecated("Use get_set_index() instead.")]] uint64_t get_set(uint64_t address) const;
  [[deprecated("This function should not be used to access the blocks directly.")]] uint64_t get_way(uint64_t address, uint64_t set) const;

  uint64_t invalidate_entry(uint64_t inval_addr);
  int prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata);

  [[deprecated("Use CACHE::prefetch_line(pf_addr, fill_this_level, prefetch_metadata) instead.")]] int
  prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata);

  bool should_activate_prefetcher(const PACKET& pkt) const;

  void print_deadlock() override;

#include "cache_modules.inc"

  const std::bitset<NUM_REPLACEMENT_MODULES> repl_type;
  const std::bitset<NUM_PREFETCH_MODULES> pref_type;

  // Uncapped internal PQ size
  CACHE(std::string v1, double freq_scale, uint32_t v2, uint32_t v3, uint32_t v8, uint64_t hit_lat, uint64_t fill_lat, long int max_tag, long int max_fill, unsigned offset_bits,
        bool pref_load, bool wq_full_addr, bool va_pref, unsigned pref_mask, std::vector<champsim::channel*>&& uls, champsim::channel* lt, champsim::channel* ll,
        std::bitset<NUM_PREFETCH_MODULES> pref, std::bitset<NUM_REPLACEMENT_MODULES> repl);

  CACHE(std::string v1, double freq_scale, uint32_t v2, uint32_t v3, uint32_t v8, std::size_t pq_size, uint64_t hit_lat, uint64_t fill_lat, long int max_tag, long int max_fill, unsigned offset_bits,
        bool pref_load, bool wq_full_addr, bool va_pref, unsigned pref_mask, std::vector<champsim::channel*>&& uls, champsim::channel* lt, champsim::channel* ll,
        std::bitset<NUM_PREFETCH_MODULES> pref, std::bitset<NUM_REPLACEMENT_MODULES> repl);
};

#endif
