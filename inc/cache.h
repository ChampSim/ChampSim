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

#ifdef CHAMPSIM_MODULE
#define SET_ASIDE_CHAMPSIM_MODULE
#undef CHAMPSIM_MODULE
#endif

#include <array>
#include <cstddef> // for size_t
#include <cstdint> // for uint64_t, uint32_t, uint8_t
#include <deque>
#include <iterator> // for size
#include <limits>   // for numeric_limits
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "address.h"
#include "bandwidth.h"
#include "block.h"
#include "cache_stats.h"
#include "champsim.h"
#include "channel.h"
#include "chrono.h"
#include "modules.h"
#include "operable.h"
#include "util/to_underlying.h" // for to_underlying
#include "waitable.h"

class CACHE : public champsim::modules::cache_module
{
  enum [[deprecated(
      "Prefetchers may not specify arbitrary fill levels. Use CACHE::prefetch_line(pf_addr, fill_this_level, prefetch_metadata) instead.")]] FILL_LEVEL{
      FILL_L1 = 1, FILL_L2 = 2, FILL_LLC = 4, FILL_DRC = 8, FILL_DRAM = 16};

  using channel_type = champsim::modules::channel_module;
  using request_type = typename channel_type::request_type;
  using response_type = typename channel_type::response_type;

  struct tag_lookup_type {
    champsim::address address;
    champsim::address v_address;
    champsim::address data;
    champsim::address ip;
    uint64_t instr_id;

    uint32_t pf_metadata;
    champsim::origin origin;

    access_type type;
    bool prefetch_from_this;
    bool skip_fill;
    bool is_translated;
    bool translate_issued = false;

    champsim::chrono::clock::time_point event_cycle = champsim::chrono::clock::time_point::max();

    std::vector<uint64_t> instr_depend_on_me{};
    std::vector<std::deque<response_type>*> to_return{};

    explicit tag_lookup_type(request_type req) : tag_lookup_type(req, false, false) {}
    tag_lookup_type(const request_type& req, bool local_pref, bool skip);
  };

public:
  struct fill_type {
    champsim::address address;
    champsim::address v_address;
    champsim::address ip;
    uint64_t instr_id;

    struct returned_value {
      champsim::address data;
      uint32_t pf_metadata;
    };
    champsim::waitable<returned_value> data_promise{};
    champsim::origin origin;

    access_type type;
    bool prefetch_from_this;

    champsim::chrono::clock::time_point time_enqueued;

    std::vector<uint64_t> instr_depend_on_me{};
    std::vector<std::deque<response_type>*> to_return{};

    fill_type(const tag_lookup_type& req, champsim::chrono::clock::time_point _time_enqueued);
    static fill_type merge(fill_type predecessor, fill_type successor);
  };

private:
  bool try_hit(const tag_lookup_type& handle_pkt);
  bool handle_fill(const fill_type& fill);
  bool handle_miss(const tag_lookup_type& handle_pkt);
  bool handle_write(const tag_lookup_type& handle_pkt);
  void finish_packet(const response_type& packet);
  void finish_translation(const response_type& packet);

  void issue_translation(tag_lookup_type& q_entry) const;

public:
  using BLOCK = champsim::cache_block;

private:
  static BLOCK fill_block(fill_type fill, uint32_t metadata);
  using set_type = std::vector<BLOCK>;

  std::pair<set_type::iterator, set_type::iterator> get_set_span(champsim::address address);
  [[nodiscard]] std::pair<set_type::const_iterator, set_type::const_iterator> get_set_span(champsim::address address) const;
  [[nodiscard]] long get_set_index(champsim::address address) const;

  template <typename T>
  bool should_activate_prefetcher(const T& pkt) const;

  template <bool>
  auto initiate_tag_check(champsim::modules::channel_module* ul = nullptr);

  template <typename T>
  champsim::address module_address(const T& element) const;

  auto matches_address(champsim::address address) const;
  std::pair<fill_type, request_type> mshr_and_forward_packet(const tag_lookup_type& handle_pkt);

  std::deque<tag_lookup_type> internal_PQ{};
  std::deque<tag_lookup_type> inflight_tag_check{};
  std::deque<tag_lookup_type> translation_stash{};

  std::vector<champsim::modules::prefetcher*> pref_module_pimpl;
  std::vector<champsim::modules::replacement*> repl_module_pimpl;

public:
  std::vector<channel_type*> upper_levels;
  channel_type* lower_level;
  channel_type* lower_translate;

  // Provenance of the most recently served packet; stamped onto prefetches
  // issued by this cache (attribution: prefetches belong to whoever touched
  // the cache last).
  champsim::origin last_served_origin{};
  std::string NAME;
  uint32_t NUM_SET, NUM_WAY, MSHR_SIZE;
  std::size_t PQ_SIZE;
  champsim::chrono::clock::duration HIT_LATENCY;
  champsim::chrono::clock::duration FILL_LATENCY;
  champsim::data::bits OFFSET_BITS;
  set_type block{static_cast<typename set_type::size_type>(NUM_SET * NUM_WAY)};
  champsim::bandwidth::maximum_type MAX_TAG, MAX_FILL;
  bool prefetch_as_load;
  bool match_offset_bits;
  bool virtual_prefetch;
  std::vector<access_type> pref_activate_mask;

  using stats_type = cache_stats;

  stats_type sim_stats;

  std::deque<fill_type> MSHR;
  std::deque<fill_type> inflight_fills;

  long operate() final;
  long poll_cycle() final;
  void initialize() final;
  void begin_phase(bool warmup) override;
  void end_phase(champsim::stat_report& out) override;
  void end_simulation() final;

private:
  // Snapshot of the warmup flag for the current phase.
  bool warmup_ = true;

public:
  bool is_warmup() const { return warmup_; }

  [[deprecated]] std::size_t get_occupancy(uint8_t queue_type, champsim::address address) const;
  [[deprecated]] std::size_t get_size(uint8_t queue_type, champsim::address address) const;

  champsim::bandwidth::maximum_type get_max_tag_bandwidth() const { return MAX_TAG; }
  stats_type get_sim_stats() const final { return sim_stats; }

  bool is_virtual_prefetch() const final { return virtual_prefetch; }

  // NOLINTBEGIN
  [[deprecated("get_occupancy() returns 0 for every input except 0 (MSHR). Use get_mshr_occupancy() instead.")]] std::size_t
  get_occupancy(uint8_t queue_type, uint64_t address) const;
  [[deprecated("get_size() returns 0 for every input except 0 (MSHR). Use get_mshr_size() instead.")]] std::size_t get_size(uint8_t queue_type,
                                                                                                                            uint64_t address) const;
  // NOLINTEND

  [[nodiscard]] std::size_t get_mshr_occupancy() const;
  [[nodiscard]] std::size_t get_mshr_size() const;
  [[nodiscard]] double get_mshr_occupancy_ratio() const;

  [[nodiscard]] std::vector<std::size_t> get_rq_occupancy() const;
  [[nodiscard]] std::vector<std::size_t> get_rq_size() const;
  [[nodiscard]] std::vector<double> get_rq_occupancy_ratio() const;

  [[nodiscard]] std::vector<std::size_t> get_wq_occupancy() const;
  [[nodiscard]] std::vector<std::size_t> get_wq_size() const;
  [[nodiscard]] std::vector<double> get_wq_occupancy_ratio() const;

  [[nodiscard]] std::vector<std::size_t> get_pq_occupancy() const;
  [[nodiscard]] std::vector<std::size_t> get_pq_size() const;
  [[nodiscard]] std::vector<double> get_pq_occupancy_ratio() const;

  [[nodiscard]] std::size_t num_sets() const { return NUM_SET; }
  [[nodiscard]] std::size_t num_ways() const { return NUM_WAY; }
  [[nodiscard]] champsim::data::bits get_offset_bits() const final { return OFFSET_BITS; }

  [[deprecated("Use get_set_index() instead.")]] [[nodiscard]] uint64_t get_set(uint64_t address) const;
  [[deprecated("This function should not be used to access the blocks directly.")]] [[nodiscard]] uint64_t get_way(uint64_t address, uint64_t set) const;

  long invalidate_entry(champsim::address inval_addr);
  bool prefetch_line(champsim::address pf_addr, bool fill_this_level, uint32_t prefetch_metadata);

  [[deprecated]] bool prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata);

  [[deprecated("Use CACHE::prefetch_line(pf_addr, fill_this_level, prefetch_metadata) instead.")]] bool
  prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata);

  void print_deadlock() final;

  void impl_prefetcher_initialize() const;
  [[nodiscard]] uint32_t impl_prefetcher_cache_operate(champsim::address addr, champsim::address ip, bool cache_hit, bool useful_prefetch, access_type type,
                                                       uint32_t metadata_in) const;
  [[nodiscard]] uint32_t impl_prefetcher_cache_fill(champsim::address addr, long set, long way, bool prefetch, champsim::address evicted_addr,
                                                    uint32_t metadata_in) const;
  void impl_prefetcher_cycle_operate() const;
  void impl_prefetcher_final_stats() const;
  void impl_prefetcher_branch_operate(champsim::address ip, uint8_t branch_type, champsim::address branch_target) const;
  void impl_initialize_replacement() const;
  [[nodiscard]] long impl_find_victim(champsim::origin origin, uint64_t instr_id, long set, const BLOCK* current_set, champsim::address ip,
                                      champsim::address full_addr, access_type type) const;
  void impl_update_replacement_state(champsim::origin origin, long set, long way, champsim::address full_addr, champsim::address ip,
                                     champsim::address victim_addr, access_type type, bool hit) const;
  void impl_replacement_cache_fill(champsim::origin origin, long set, long way, champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type) const;
  void impl_replacement_final_stats() const;
  // NOLINTEND(readability-make-member-function-const)

  explicit CACHE(champsim::modules::ModuleBuilder builder)
      : champsim::modules::cache_module(builder.get_parameter<champsim::chrono::picoseconds>("clock_period")),
        upper_levels(builder.get_parameter<std::vector<champsim::modules::channel_module*>>("upper_levels")),
        lower_level(builder.get_parameter<champsim::modules::channel_module*>("lower_level")),
        lower_translate(builder.get_parameter<champsim::modules::channel_module*>("lower_translate")), NAME(builder.get_name()),
        NUM_SET(builder.get_parameter<uint32_t>("num_sets")), NUM_WAY(builder.get_parameter<uint32_t>("num_ways")),
        MSHR_SIZE(builder.get_parameter<uint32_t>("mshr_size")), PQ_SIZE(builder.get_parameter<std::size_t>("pq_size")),
        HIT_LATENCY(builder.get_parameter<uint64_t>("hit_latency") * builder.get_parameter<champsim::chrono::picoseconds>("clock_period")),
        FILL_LATENCY(builder.get_parameter<uint64_t>("fill_latency") * builder.get_parameter<champsim::chrono::picoseconds>("clock_period")),
        OFFSET_BITS(builder.get_parameter<champsim::data::bits>("offset_bits")),
        MAX_TAG(builder.get_parameter<champsim::bandwidth::maximum_type>("max_tag_bandwidth")),
        MAX_FILL(builder.get_parameter<champsim::bandwidth::maximum_type>("max_fill_bandwidth")),
        prefetch_as_load(builder.get_parameter<bool>("prefetch_as_load")), match_offset_bits(builder.get_parameter<bool>("match_offset_bits")),
        virtual_prefetch(builder.get_parameter<bool>("virtual_prefetch")),
        pref_activate_mask(builder.get_parameter<std::vector<access_type>>("pref_activate_mask"))
  {
    // Construct prefetcher submodules
    for (const auto& sub : builder.get_submodules("prefetcher", true))
      pref_module_pimpl.push_back(champsim::modules::prefetcher::create_instance(sub, static_cast<champsim::modules::cache_module*>(this)));

    // Construct replacement submodules
    for (const auto& sub : builder.get_submodules("replacement"))
      repl_module_pimpl.push_back(champsim::modules::replacement::create_instance(sub, static_cast<champsim::modules::cache_module*>(this)));
  }

  CACHE(const CACHE&) = delete;
  CACHE(CACHE&&);
  CACHE& operator=(const CACHE&) = delete;
  CACHE& operator=(CACHE&&);
};

#ifdef SET_ASIDE_CHAMPSIM_MODULE
#undef SET_ASIDE_CHAMPSIM_MODULE
#define CHAMPSIM_MODULE
#endif

#endif
