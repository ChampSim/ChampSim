/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

  using channel_type = champsim::channel;
  using request_type = typename channel_type::request_type;
  using response_type = typename channel_type::response_type;

  struct tag_lookup_type {
    uint64_t address;
    uint64_t v_address;
    uint64_t data;
    uint64_t ip;
    uint64_t instr_id;

    uint32_t pf_metadata;
    uint32_t cpu;

    uint8_t type;
    bool prefetch_from_this;
    bool skip_fill;
    bool is_translated;
    bool translate_issued = false;

    uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};

    uint64_t event_cycle = std::numeric_limits<uint64_t>::max();

    std::vector<std::reference_wrapper<ooo_model_instr>> instr_depend_on_me{};
    std::vector<std::deque<response_type>*> to_return{};

    explicit tag_lookup_type(request_type req) : tag_lookup_type(req, false, false) {}
    tag_lookup_type(request_type req, bool local_pref, bool skip);
  };

  struct mshr_type {
    uint64_t address;
    uint64_t v_address;
    uint64_t data;
    uint64_t ip;
    uint64_t instr_id;

    uint32_t pf_metadata;
    uint32_t cpu;

    uint8_t type;
    bool prefetch_from_this;

    uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};

    uint64_t event_cycle = std::numeric_limits<uint64_t>::max();
    uint64_t cycle_enqueued;

    std::vector<std::reference_wrapper<ooo_model_instr>> instr_depend_on_me{};
    std::vector<std::deque<response_type>*> to_return{};

    mshr_type(tag_lookup_type req, uint64_t cycle);
  };

  bool try_hit(const tag_lookup_type& handle_pkt);
  bool handle_fill(const mshr_type& fill_mshr);
  bool handle_miss(const tag_lookup_type& handle_pkt);
  bool handle_write(const tag_lookup_type& handle_pkt);
  void finish_packet(const response_type& packet);
  void finish_translation(const response_type& packet);

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

    BLOCK() = default;
    explicit BLOCK(mshr_type mshr);
  };
  using set_type = std::vector<BLOCK>;

  std::pair<set_type::iterator, set_type::iterator> get_set_span(uint64_t address);
  std::pair<set_type::const_iterator, set_type::const_iterator> get_set_span(uint64_t address) const;
  std::size_t get_set_index(uint64_t address) const;

  template <typename T>
  bool should_activate_prefetcher(const T& pkt) const;

  std::deque<tag_lookup_type> internal_PQ{};
  std::deque<tag_lookup_type> inflight_tag_check{};
  std::deque<tag_lookup_type> translation_stash{};

public:
  std::vector<channel_type*> upper_levels;
  channel_type* lower_level;
  channel_type* lower_translate;

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

  std::deque<mshr_type> MSHR;
  std::deque<mshr_type> inflight_writes;

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

  void print_deadlock() override;

#include "cache_modules.inc"

  const std::bitset<NUM_REPLACEMENT_MODULES> repl_type;
  const std::bitset<NUM_PREFETCH_MODULES> pref_type;

  class Builder
  {
      std::string_view m_name{};
      double m_freq_scale{};
      uint32_t m_sets{};
      uint32_t m_ways{};
      std::size_t m_pq_size{std::numeric_limits<std::size_t>::max()};
      uint32_t m_mshr_size{};
      uint32_t m_hit_lat{};
      uint32_t m_fill_lat{};
      uint32_t m_max_tag{};
      uint32_t m_max_fill{};
      std::size_t m_offset_bits{};
      bool m_pref_load{};
      bool m_wq_full_addr{};
      bool m_va_pref{};

      unsigned m_pref_act_mask{};
      std::vector<CACHE::channel_type*> m_uls{};
      CACHE::channel_type* m_ll{};
      CACHE::channel_type* m_lt{nullptr};
      std::bitset<NUM_PREFETCH_MODULES> m_pref{};
      std::bitset<NUM_REPLACEMENT_MODULES> m_repl{};

      friend class CACHE;

      public:

      Builder& name(std::string_view name_) { m_name = name_; return *this; }
      Builder& frequency(double freq_scale_) { m_freq_scale = freq_scale_; return *this; }
      Builder& sets(uint32_t sets_) { m_sets = sets_; return *this; }
      Builder& ways(uint32_t ways_) { m_ways = ways_; return *this; }
      Builder& pq_size(uint32_t pq_size_) { m_pq_size = pq_size_; return *this; }
      Builder& mshr_size(uint32_t mshr_size_) { m_mshr_size = mshr_size_; return *this; }
      Builder& latency(uint32_t lat_) { m_fill_lat = lat_/2; m_hit_lat = lat_ - m_fill_lat; return *this; }
      Builder& hit_latency(uint32_t hit_lat_) { m_hit_lat = hit_lat_; return *this; }
      Builder& fill_latency(uint32_t fill_lat_) { m_fill_lat = fill_lat_; return *this; }
      Builder& tag_bandwidth(uint32_t max_read_) { m_max_tag = max_read_; return *this; }
      Builder& fill_bandwidth(uint32_t max_write_) { m_max_fill = max_write_; return *this; }
      Builder& offset_bits(std::size_t offset_bits_) { m_offset_bits = offset_bits_; return *this; }
      Builder& set_prefetch_as_load(bool pref_load_) { m_pref_load = pref_load_; return *this; }
      Builder& set_wq_checks_full_addr(bool wq_full_addr_) { m_wq_full_addr = wq_full_addr_; return *this; }
      Builder& set_virtual_prefetch(bool va_pref_) { m_va_pref = va_pref_; return *this; }
      Builder& prefetch_activate(unsigned pref_act_mask_) { m_pref_act_mask = pref_act_mask_; return *this; }
      Builder& upper_levels(std::vector<CACHE::channel_type*>&& uls_) { m_uls = std::move(uls_); return *this; }
      Builder& lower_level(CACHE::channel_type* ll_) { m_ll = ll_; return *this; }
      Builder& lower_translate(CACHE::channel_type* lt_) { m_lt = lt_; return *this; }
      Builder& prefetcher(std::bitset<NUM_PREFETCH_MODULES> pref_) { m_pref = pref_; return *this; }
      Builder& replacement(std::bitset<NUM_REPLACEMENT_MODULES> repl_) { m_repl = repl_; return *this; }
  };

  CACHE(Builder b);
};

#endif
