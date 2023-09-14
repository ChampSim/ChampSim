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

#ifdef CHAMPSIM_MODULE
#define SET_ASIDE_CHAMPSIM_MODULE
#undef CHAMPSIM_MODULE
#endif

#ifndef CACHE_H
#define CACHE_H

#include <array>
#include <cstddef> // for size_t
#include <cstdint> // for uint64_t, uint32_t, uint8_t
#include <deque>
#include <iterator> // for size
#include <limits>   // for numeric_limits
#include <memory>
#include <stdexcept>
#include <string>
#include <utility> // for pair
#include <vector>

#include "block.h"
#include "cache_builder.h"
#include "champsim_constants.h"
#include "channel.h"
#include "modules.h"
#include "operable.h"
#include "util/to_underlying.h" // for to_underlying
#include "waitable.h"

struct ooo_model_instr;

struct cache_stats {
  std::string name;
  // prefetch stats
  uint64_t pf_requested = 0;
  uint64_t pf_issued = 0;
  uint64_t pf_useful = 0;
  uint64_t pf_useless = 0;
  uint64_t pf_fill = 0;

  std::array<std::array<uint64_t, NUM_CPUS>, champsim::to_underlying(access_type::NUM_TYPES)> hits = {};
  std::array<std::array<uint64_t, NUM_CPUS>, champsim::to_underlying(access_type::NUM_TYPES)> misses = {};

  double avg_miss_latency = 0;
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

    access_type type;
    bool prefetch_from_this;
    bool skip_fill;
    bool is_translated;
    bool translate_issued = false;

    uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};

    uint64_t event_cycle = std::numeric_limits<uint64_t>::max();

    std::vector<uint64_t> instr_depend_on_me{};
    std::vector<std::deque<response_type>*> to_return{};

    explicit tag_lookup_type(request_type req) : tag_lookup_type(req, false, false) {}
    tag_lookup_type(const request_type& req, bool local_pref, bool skip);
  };

  struct mshr_type {
    uint64_t address;
    uint64_t v_address;
    uint64_t ip;
    uint64_t instr_id;

    struct returned_value {
      uint64_t data;
      uint32_t pf_metadata;
    };
    champsim::waitable<returned_value> data_promise{};
    uint32_t cpu;

    access_type type;
    bool prefetch_from_this;

    uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};

    uint64_t cycle_enqueued;

    std::vector<uint64_t> instr_depend_on_me{};
    std::vector<std::deque<response_type>*> to_return{};

    mshr_type(const tag_lookup_type& req, uint64_t cycle);
    static mshr_type merge(mshr_type predecessor, mshr_type successor);
  };

  bool try_hit(const tag_lookup_type& handle_pkt);
  bool handle_fill(const mshr_type& fill_mshr);
  bool handle_miss(const tag_lookup_type& handle_pkt);
  bool handle_write(const tag_lookup_type& handle_pkt);
  void finish_packet(const response_type& packet);
  void finish_translation(const response_type& packet);

  void issue_translation(tag_lookup_type& q_entry) const;

public:
  using BLOCK = champsim::cache_block;

private:
  static BLOCK fill_block(mshr_type mshr, uint32_t metadata);
  using set_type = std::vector<BLOCK>;

  std::pair<set_type::iterator, set_type::iterator> get_set_span(uint64_t address);
  [[nodiscard]] std::pair<set_type::const_iterator, set_type::const_iterator> get_set_span(uint64_t address) const;
  [[nodiscard]] long get_set_index(uint64_t address) const;

  template <typename T>
  bool should_activate_prefetcher(const T& pkt) const;

  template <bool>
  auto initiate_tag_check(champsim::channel* ul = nullptr);

  template <typename T>
  uint64_t module_address(const T& element) const;

  auto matches_address(uint64_t address) const;
  std::pair<mshr_type, request_type> mshr_and_forward_packet(const tag_lookup_type& handle_pkt);

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
  set_type block{static_cast<typename set_type::size_type>(NUM_SET * NUM_WAY)};
  const long int MAX_TAG, MAX_FILL;
  const bool prefetch_as_load;
  const bool match_offset_bits;
  const bool virtual_prefetch;
  const std::vector<access_type> pref_activate_mask;

  using stats_type = cache_stats;

  stats_type sim_stats, roi_stats;

  std::deque<mshr_type> MSHR;
  std::deque<mshr_type> inflight_writes;

  long operate() final;

  void initialize() final;
  void begin_phase() final;
  void end_phase(unsigned cpu) final;

  [[deprecated("get_occupancy() returns 0 for every input except 0 (MSHR). Use get_mshr_occupancy() instead.")]] [[nodiscard]] std::size_t
  get_occupancy(uint8_t queue_type, uint64_t address) const;
  [[deprecated("get_size() returns 0 for every input except 0 (MSHR). Use get_mshr_size() instead.")]] [[nodiscard]] std::size_t
  get_size(uint8_t queue_type, uint64_t address) const;

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

  [[deprecated("Use get_set_index() instead.")]] [[nodiscard]] uint64_t get_set(uint64_t address) const;
  [[deprecated("This function should not be used to access the blocks directly.")]] [[nodiscard]] uint64_t get_way(uint64_t address, uint64_t set) const;

  long invalidate_entry(uint64_t inval_addr);
  bool prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata);

  [[deprecated("Use CACHE::prefetch_line(pf_addr, fill_this_level, prefetch_metadata) instead.")]] bool
  prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata);

  void print_deadlock() final;

#if __has_include("cache_module_decl.inc")
#include "cache_module_decl.inc"
#endif

  struct prefetcher_module_concept {
    virtual ~prefetcher_module_concept() = default;

    virtual void impl_prefetcher_initialize() = 0;
    virtual uint32_t impl_prefetcher_cache_operate(uint64_t addr, uint64_t ip, bool cache_hit, bool useful_prefetch, access_type type,
                                                   uint32_t metadata_in) = 0;
    virtual uint32_t impl_prefetcher_cache_fill(uint64_t addr, long set, long way, bool prefetch, uint64_t evicted_addr, uint32_t metadata_in) = 0;
    virtual void impl_prefetcher_cycle_operate() = 0;
    virtual void impl_prefetcher_final_stats() = 0;
    virtual void impl_prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target) = 0;
  };

  struct replacement_module_concept {
    virtual ~replacement_module_concept() = default;

    virtual void impl_initialize_replacement() = 0;
    virtual long impl_find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr,
                                  access_type type) = 0;
    virtual void impl_update_replacement_state(uint32_t triggering_cpu, long set, long way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr,
                                               access_type type, bool hit) = 0;
    virtual void impl_replacement_final_stats() = 0;
  };

  template <typename... Ps>
  struct prefetcher_module_model final : prefetcher_module_concept {
    std::tuple<Ps...> intern_;
    explicit prefetcher_module_model(CACHE* cache) : intern_(Ps{cache}...) { (void)cache; /* silence -Wunused-but-set-parameter when sizeof...(Ps) == 0 */ }

    void impl_prefetcher_initialize() final;
    [[nodiscard]] uint32_t impl_prefetcher_cache_operate(uint64_t addr, uint64_t ip, bool cache_hit, bool useful_prefetch, access_type type,
                                                         uint32_t metadata_in) final;
    [[nodiscard]] uint32_t impl_prefetcher_cache_fill(uint64_t addr, long set, long way, bool prefetch, uint64_t evicted_addr, uint32_t metadata_in) final;
    void impl_prefetcher_cycle_operate() final;
    void impl_prefetcher_final_stats() final;
    void impl_prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target) final;
  };

  template <typename... Rs>
  struct replacement_module_model final : replacement_module_concept {
    // Assert that at least one has an update state
    // static_assert(std::disjunction<champsim::is_detected<has_update_state, Rs>...>::value, "At least one replacement policy must update its state");

    std::tuple<Rs...> intern_;
    explicit replacement_module_model(CACHE* cache) : intern_(Rs{cache}...) { (void)cache; /* silence -Wunused-but-set-parameter when sizeof...(Rs) == 0 */ }

    void impl_initialize_replacement() final;
    [[nodiscard]] long impl_find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr,
                                        access_type type) final;
    void impl_update_replacement_state(uint32_t triggering_cpu, long set, long way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, access_type type,
                                       bool hit) final;
    void impl_replacement_final_stats() final;
  };

  std::unique_ptr<prefetcher_module_concept> pref_module_pimpl;
  std::unique_ptr<replacement_module_concept> repl_module_pimpl;

  // NOLINTBEGIN(readability-make-member-function-const): legacy modules use non-const hooks
  void impl_prefetcher_initialize() const;
  [[nodiscard]] uint32_t impl_prefetcher_cache_operate(uint64_t addr, uint64_t ip, bool cache_hit, bool useful_prefetch, access_type type,
                                                       uint32_t metadata_in) const;
  [[nodiscard]] uint32_t impl_prefetcher_cache_fill(uint64_t addr, long set, long way, bool prefetch, uint64_t evicted_addr, uint32_t metadata_in) const;
  void impl_prefetcher_cycle_operate() const;
  void impl_prefetcher_final_stats() const;
  void impl_prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target) const;

  void impl_initialize_replacement() const;
  [[nodiscard]] long impl_find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr,
                                      access_type type) const;
  void impl_update_replacement_state(uint32_t triggering_cpu, long set, long way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, access_type type,
                                     bool hit) const;
  void impl_replacement_final_stats() const;
  // NOLINTEND(readability-make-member-function-const)

  template <typename... Ps, typename... Rs>
  explicit CACHE(champsim::cache_builder<champsim::cache_builder_module_type_holder<Ps...>, champsim::cache_builder_module_type_holder<Rs...>> b)
      : champsim::operable(b.m_freq_scale), upper_levels(b.m_uls), lower_level(b.m_ll), lower_translate(b.m_lt), NAME(b.m_name), NUM_SET(b.get_num_sets()),
        NUM_WAY(b.get_num_ways()), MSHR_SIZE(b.get_num_mshrs()), PQ_SIZE(b.m_pq_size), HIT_LATENCY(b.m_hit_lat.value_or(b.m_latency - b.m_fill_lat)),
        FILL_LATENCY(b.m_fill_lat), OFFSET_BITS(b.m_offset_bits), MAX_TAG(b.get_tag_bandwidth()), MAX_FILL(b.get_fill_bandwidth()),
        prefetch_as_load(b.m_pref_load), match_offset_bits(b.m_wq_full_addr), virtual_prefetch(b.m_va_pref), pref_activate_mask(b.m_pref_act_mask),
        pref_module_pimpl(std::make_unique<prefetcher_module_model<Ps...>>(this)), repl_module_pimpl(std::make_unique<replacement_module_model<Rs...>>(this))
  {
  }
};

template <typename... Ps>
void CACHE::prefetcher_module_model<Ps...>::impl_prefetcher_initialize()
{
  [[maybe_unused]] auto process_one = [&](auto& p) {
    using namespace champsim::modules;
    if constexpr (prefetcher::has_initialize<decltype(p)>)
      p.prefetcher_initialize();
  };

  std::apply([&](auto&... p) { (..., process_one(p)); }, intern_);
}

template <typename... Ps>
uint32_t CACHE::prefetcher_module_model<Ps...>::impl_prefetcher_cache_operate(uint64_t addr, uint64_t ip, bool cache_hit, bool useful_prefetch,
                                                                              access_type type, uint32_t metadata_in)
{
  [[maybe_unused]] auto process_one = [&](auto& p) {
    using namespace champsim::modules;
    if constexpr (prefetcher::has_cache_operate<decltype(p), uint64_t, uint64_t, bool, bool, access_type, uint32_t>)
      return p.prefetcher_cache_operate(addr, ip, cache_hit, useful_prefetch, type, metadata_in);
    if constexpr (prefetcher::has_cache_operate<decltype(p), uint64_t, uint64_t, bool, bool, std::underlying_type_t<access_type>, uint32_t>)
      return p.prefetcher_cache_operate(addr, ip, cache_hit, useful_prefetch, champsim::to_underlying(type), metadata_in);
    if constexpr (prefetcher::has_cache_operate<decltype(p), uint64_t, uint64_t, bool, std::underlying_type_t<access_type>, uint32_t>)
      return p.prefetcher_cache_operate(addr, ip, cache_hit, champsim::to_underlying(type), metadata_in); // absent useful_prefetch
    return 0u;
  };

  return std::apply([&](auto&... p) { return (0u ^ ... ^ process_one(p)); }, intern_);
}

template <typename... Ps>
uint32_t CACHE::prefetcher_module_model<Ps...>::impl_prefetcher_cache_fill(uint64_t addr, long set, long way, bool prefetch, uint64_t evicted_addr,
                                                                           uint32_t metadata_in)
{
  [[maybe_unused]] auto process_one = [&](auto& p) {
    using namespace champsim::modules;
    if constexpr (prefetcher::has_cache_fill<decltype(p), uint64_t, long, long, bool, uint64_t, uint32_t>)
      return p.prefetcher_cache_fill(addr, set, way, prefetch, evicted_addr, metadata_in);
    return 0u;
  };

  return std::apply([&](auto&... p) { return (0u ^ ... ^ process_one(p)); }, intern_);
}

template <typename... Ps>
void CACHE::prefetcher_module_model<Ps...>::impl_prefetcher_cycle_operate()
{
  [[maybe_unused]] auto process_one = [&](auto& p) {
    using namespace champsim::modules;
    if constexpr (prefetcher::has_cycle_operate<decltype(p)>)
      p.prefetcher_cycle_operate();
  };

  std::apply([&](auto&... p) { (..., process_one(p)); }, intern_);
}

template <typename... Ps>
void CACHE::prefetcher_module_model<Ps...>::impl_prefetcher_final_stats()
{
  [[maybe_unused]] auto process_one = [&](auto& p) {
    using namespace champsim::modules;
    if constexpr (prefetcher::has_final_stats<decltype(p)>)
      p.prefetcher_final_stats();
  };

  std::apply([&](auto&... p) { (..., process_one(p)); }, intern_);
}

template <typename... Ps>
void CACHE::prefetcher_module_model<Ps...>::impl_prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target)
{
  [[maybe_unused]] auto process_one = [&](auto& p) {
    using namespace champsim::modules;
    if constexpr (prefetcher::has_branch_operate<decltype(p), uint64_t, uint8_t, uint64_t>)
      p.prefetcher_branch_operate(ip, branch_type, branch_target);
  };

  std::apply([&](auto&... p) { (..., process_one(p)); }, intern_);
}

template <typename... Rs>
void CACHE::replacement_module_model<Rs...>::impl_initialize_replacement()
{
  [[maybe_unused]] auto process_one = [&](auto& r) {
    using namespace champsim::modules;
    if constexpr (replacement::has_initialize<decltype(r)>)
      r.initialize_replacement();
  };

  std::apply([&](auto&... r) { (..., process_one(r)); }, intern_);
}

template <typename... Rs>
long CACHE::replacement_module_model<Rs...>::impl_find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const BLOCK* current_set, uint64_t ip,
                                                              uint64_t full_addr, access_type type)
{
  [[maybe_unused]] auto process_one = [&](auto& r) {
    using namespace champsim::modules;
    if constexpr (replacement::has_find_victim<decltype(r), uint32_t, uint64_t, long, const BLOCK*, uint64_t, uint64_t, access_type>)
      return r.find_victim(triggering_cpu, instr_id, set, current_set, ip, full_addr, type);
    if constexpr (replacement::has_find_victim<decltype(r), uint32_t, uint64_t, long, const BLOCK*, uint64_t, uint64_t, std::underlying_type_t<access_type>>)
      return r.find_victim(triggering_cpu, instr_id, set, current_set, ip, full_addr, champsim::to_underlying(type));
    return 0L;
  };

  if constexpr (sizeof...(Rs) > 0) {
    return std::apply([&](auto&... r) { return (..., process_one(r)); }, intern_);
  }
  return 0;
}

template <typename... Rs>
void CACHE::replacement_module_model<Rs...>::impl_update_replacement_state(uint32_t triggering_cpu, long set, long way, uint64_t full_addr, uint64_t ip,
                                                                           uint64_t victim_addr, access_type type, bool hit)
{
  [[maybe_unused]] auto process_one = [&](auto& r) {
    using namespace champsim::modules;
    if constexpr (replacement::has_update_state<decltype(r), uint32_t, long, long, uint64_t, uint64_t, uint64_t, access_type, bool>)
      r.update_replacement_state(triggering_cpu, set, way, full_addr, ip, victim_addr, type, hit);
    if constexpr (replacement::has_update_state<decltype(r), uint32_t, long, long, uint64_t, uint64_t, uint64_t, std::underlying_type_t<access_type>, bool>)
      r.update_replacement_state(triggering_cpu, set, way, full_addr, ip, victim_addr, champsim::to_underlying(type), hit);
  };

  std::apply([&](auto&... r) { (..., process_one(r)); }, intern_);
}

template <typename... Rs>
void CACHE::replacement_module_model<Rs...>::impl_replacement_final_stats()
{
  [[maybe_unused]] auto process_one = [&](auto& r) {
    using namespace champsim::modules;
    if constexpr (replacement::has_final_stats<decltype(r)>)
      r.replacement_final_stats();
  };

  std::apply([&](auto&... r) { (..., process_one(r)); }, intern_);
}
#endif

#ifdef SET_ASIDE_CHAMPSIM_MODULE
#undef SET_ASIDE_CHAMPSIM_MODULE
#define CHAMPSIM_MODULE
#endif
