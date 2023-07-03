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
#include <bitset>
#include <deque>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "address.h"
#include "block.h"
#include "champsim.h"
#include "champsim_constants.h"
#include "channel.h"
#include "modules_detect.h"
#include "operable.h"

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
    champsim::address address;
    champsim::address v_address;
    champsim::address data;
    champsim::address ip;
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

    std::vector<std::reference_wrapper<ooo_model_instr>> instr_depend_on_me{};
    std::vector<std::deque<response_type>*> to_return{};

    explicit tag_lookup_type(request_type req) : tag_lookup_type(req, false, false) {}
    tag_lookup_type(const request_type& req, bool local_pref, bool skip);
  };

public:
  struct mshr_type {
    champsim::address address;
    champsim::address v_address;
    champsim::address data;
    champsim::address ip;
    uint64_t instr_id;

    uint32_t pf_metadata;
    uint32_t cpu;

    access_type type;
    bool prefetch_from_this;

    uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};

    uint64_t event_cycle = std::numeric_limits<uint64_t>::max();
    uint64_t cycle_enqueued;

    std::vector<std::reference_wrapper<ooo_model_instr>> instr_depend_on_me{};
    std::vector<std::deque<response_type>*> to_return{};

    mshr_type(const tag_lookup_type& req, uint64_t cycle);
  };

private:
  bool try_hit(const tag_lookup_type& handle_pkt);
  bool handle_fill(const mshr_type& fill_mshr);
  bool handle_miss(const tag_lookup_type& handle_pkt);
  bool handle_write(const tag_lookup_type& handle_pkt);
  void finish_packet(const response_type& packet);
  void finish_translation(const response_type& packet);

  void issue_translation();

public:
  using BLOCK = champsim::cache_block;

private:
  static BLOCK fill_block(mshr_type mshr, uint32_t metadata);
  using set_type = std::vector<BLOCK>;

  std::pair<set_type::iterator, set_type::iterator> get_set_span(champsim::address address);
  [[nodiscard]] std::pair<set_type::const_iterator, set_type::const_iterator> get_set_span(champsim::address address) const;
  [[nodiscard]] long get_set_index(champsim::address address) const;

  template <typename T>
  bool should_activate_prefetcher(const T& pkt) const;

  template <bool>
  auto initiate_tag_check(champsim::channel* ul = nullptr);

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

  void operate() final;

  void initialize() final;
  void begin_phase() final;
  void end_phase(unsigned cpu) final;

  [[deprecated]] std::size_t get_occupancy(uint8_t queue_type, champsim::address address) const;
  [[deprecated]] std::size_t get_size(uint8_t queue_type, champsim::address address) const;

  //NOLINTBEGIN
  [[deprecated("get_occupancy() returns 0 for every input except 0 (MSHR). Use get_mshr_occupancy() instead.")]] std::size_t get_occupancy(uint8_t queue_type,
                                                                                                                                           uint64_t address) const;
  [[deprecated("get_size() returns 0 for every input except 0 (MSHR). Use get_mshr_size() instead.")]] std::size_t get_size(uint8_t queue_type,
                                                                                                                            uint64_t address) const;
  //NOLINTEND

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

  long invalidate_entry(champsim::address inval_addr);
  bool prefetch_line(champsim::address pf_addr, bool fill_this_level, uint32_t prefetch_metadata);

  [[deprecated]] bool prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata);

  [[deprecated("Use CACHE::prefetch_line(pf_addr, fill_this_level, prefetch_metadata) instead.")]] bool
  prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata);

  void print_deadlock() final;

#include "cache_module_decl.inc"

  struct prefetcher_module_concept {
    virtual ~prefetcher_module_concept() = default;

    virtual void impl_prefetcher_initialize() = 0;
    virtual uint32_t impl_prefetcher_cache_operate(champsim::address addr, champsim::address ip, bool cache_hit, bool useful_prefetch, access_type type, uint32_t metadata_in) = 0;
    virtual uint32_t impl_prefetcher_cache_fill(champsim::address addr, long set, long way, bool prefetch, champsim::address evicted_addr, uint32_t metadata_in) = 0;
    virtual void impl_prefetcher_cycle_operate() = 0;
    virtual void impl_prefetcher_final_stats() = 0;
    virtual void impl_prefetcher_branch_operate(champsim::address ip, uint8_t branch_type, champsim::address branch_target) = 0;
  };

  struct replacement_module_concept {
    virtual ~replacement_module_concept() = default;

    virtual void impl_initialize_replacement() = 0;
    virtual long impl_find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const BLOCK* current_set, champsim::address ip, champsim::address full_addr,
                                      access_type type) = 0;
    virtual void impl_update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                               access_type type, uint8_t hit) = 0;
    virtual void impl_replacement_final_stats() = 0;
  };

  template <typename... Ps>
  struct prefetcher_module_model final : prefetcher_module_concept {
    std::tuple<Ps...> intern_;
    explicit prefetcher_module_model(CACHE* cache) : intern_(Ps{cache}...) {}

    void impl_prefetcher_initialize() final;
    [[nodiscard]] uint32_t impl_prefetcher_cache_operate(champsim::address addr, champsim::address ip, bool cache_hit, bool useful_prefetch, access_type type, uint32_t metadata_in) final;
    [[nodiscard]] uint32_t impl_prefetcher_cache_fill(champsim::address addr, long set, long way, bool prefetch, champsim::address evicted_addr, uint32_t metadata_in) final;
    void impl_prefetcher_cycle_operate() final;
    void impl_prefetcher_final_stats() final;
    void impl_prefetcher_branch_operate(champsim::address ip, uint8_t branch_type, champsim::address branch_target) final;
  };

  template <typename... Rs>
  struct replacement_module_model final : replacement_module_concept {
    // Assert that at least one has an update state
    // static_assert(std::disjunction<champsim::is_detected<has_update_state, Rs>...>::value, "At least one replacement policy must update its state");

    std::tuple<Rs...> intern_;
    explicit replacement_module_model(CACHE* cache) : intern_(Rs{cache}...) {}

    void impl_initialize_replacement() final;
    [[nodiscard]] long impl_find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const BLOCK* current_set, champsim::address ip, champsim::address full_addr,
                                            access_type type) final;
    void impl_update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                       access_type type, uint8_t hit) final;
    void impl_replacement_final_stats() final;
  };

  std::unique_ptr<prefetcher_module_concept> pref_module_pimpl;
  std::unique_ptr<replacement_module_concept> repl_module_pimpl;

  // NOLINTBEGIN(readability-make-member-function-const): legacy modules use non-const hooks
  void impl_prefetcher_initialize() const;
  [[nodiscard]] uint32_t impl_prefetcher_cache_operate(champsim::address addr, champsim::address ip, bool cache_hit, bool useful_prefetch, access_type type, uint32_t metadata_in) const;
  [[nodiscard]] uint32_t impl_prefetcher_cache_fill(champsim::address addr, long set, long way, bool prefetch, champsim::address evicted_addr, uint32_t metadata_in) const;
  void impl_prefetcher_cycle_operate() const;
  void impl_prefetcher_final_stats() const;
  void impl_prefetcher_branch_operate(champsim::address ip, uint8_t branch_type, champsim::address branch_target) const;

  void impl_initialize_replacement() const;
  [[nodiscard]] long impl_find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const BLOCK* current_set, champsim::address ip, champsim::address full_addr,
                                          access_type type) const;
  void impl_update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr, access_type type,
                                     uint8_t hit) const;
  void impl_replacement_final_stats() const;
  // NOLINTEND(readability-make-member-function-const)

  template <typename... Ts>
  class builder_module_type_holder
  {
  };
  class builder_conversion_tag
  {
  };
  template <typename P = void, typename R = void>
  class Builder
  {
    using self_type = Builder<P, R>;

    std::string m_name{};
    double m_freq_scale{};
    uint32_t m_sets{};
    uint32_t m_ways{};
    std::size_t m_pq_size{std::numeric_limits<std::size_t>::max()};
    uint32_t m_mshr_size{};
    uint64_t m_hit_lat{};
    uint64_t m_fill_lat{};
    uint64_t m_latency{};
    uint32_t m_max_tag{};
    uint32_t m_max_fill{};
    unsigned m_offset_bits{};
    bool m_pref_load{};
    bool m_wq_full_addr{};
    bool m_va_pref{};

    std::vector<access_type> m_pref_act_mask{access_type::LOAD, access_type::PREFETCH};
    std::vector<CACHE::channel_type*> m_uls{};
    CACHE::channel_type* m_ll{};
    CACHE::channel_type* m_lt{nullptr};

    friend class CACHE;

    template <typename OTHER_P, typename OTHER_R>
    Builder(builder_conversion_tag /*tag*/, const Builder<OTHER_P, OTHER_R>& other)
        : m_name(other.m_name), m_freq_scale(other.m_freq_scale), m_sets(other.m_sets), m_ways(other.m_ways), m_pq_size(other.m_pq_size),
          m_mshr_size(other.m_mshr_size), m_hit_lat(other.m_hit_lat), m_fill_lat(other.m_fill_lat), m_latency(other.m_latency), m_max_tag(other.m_max_tag),
          m_max_fill(other.m_max_fill), m_offset_bits(other.m_offset_bits), m_pref_load(other.m_pref_load), m_wq_full_addr(other.m_wq_full_addr),
          m_va_pref(other.m_va_pref), m_pref_act_mask(other.m_pref_act_mask), m_uls(other.m_uls), m_ll(other.m_ll), m_lt(other.m_lt)
    {
    }

  public:
    Builder() = default;

    self_type& name(std::string name_)
    {
      m_name = name_;
      return *this;
    }
    self_type& frequency(double freq_scale_)
    {
      m_freq_scale = freq_scale_;
      return *this;
    }
    self_type& sets(uint32_t sets_)
    {
      m_sets = sets_;
      return *this;
    }
    self_type& ways(uint32_t ways_)
    {
      m_ways = ways_;
      return *this;
    }
    self_type& pq_size(uint32_t pq_size_)
    {
      m_pq_size = pq_size_;
      return *this;
    }
    self_type& mshr_size(uint32_t mshr_size_)
    {
      m_mshr_size = mshr_size_;
      return *this;
    }
    self_type& latency(uint64_t lat_)
    {
      m_latency = lat_;
      return *this;
    }
    self_type& hit_latency(uint64_t hit_lat_)
    {
      m_hit_lat = hit_lat_;
      return *this;
    }
    self_type& fill_latency(uint64_t fill_lat_)
    {
      m_fill_lat = fill_lat_;
      return *this;
    }
    self_type& tag_bandwidth(uint32_t max_read_)
    {
      m_max_tag = max_read_;
      return *this;
    }
    self_type& fill_bandwidth(uint32_t max_write_)
    {
      m_max_fill = max_write_;
      return *this;
    }
    self_type& offset_bits(unsigned offset_bits_)
    {
      m_offset_bits = offset_bits_;
      return *this;
    }
    self_type& set_prefetch_as_load()
    {
      m_pref_load = true;
      return *this;
    }
    self_type& reset_prefetch_as_load()
    {
      m_pref_load = false;
      return *this;
    }
    self_type& set_wq_checks_full_addr()
    {
      m_wq_full_addr = true;
      return *this;
    }
    self_type& reset_wq_checks_full_addr()
    {
      m_wq_full_addr = false;
      return *this;
    }
    self_type& set_virtual_prefetch()
    {
      m_va_pref = true;
      return *this;
    }
    self_type& reset_virtual_prefetch()
    {
      m_va_pref = false;
      return *this;
    }
    template <typename... Elems>
    self_type& prefetch_activate(Elems... pref_act_elems)
    {
      m_pref_act_mask = {pref_act_elems...};
      return *this;
    }
    self_type& upper_levels(std::vector<CACHE::channel_type*>&& uls_)
    {
      m_uls = std::move(uls_);
      return *this;
    }
    self_type& lower_level(CACHE::channel_type* ll_)
    {
      m_ll = ll_;
      return *this;
    }
    self_type& lower_translate(CACHE::channel_type* lt_)
    {
      m_lt = lt_;
      return *this;
    }
    template <typename... Ps>
    Builder<builder_module_type_holder<Ps...>, R> prefetcher()
    {
      return {builder_conversion_tag{}, *this};
    }
    template <typename... Rs>
    Builder<P, builder_module_type_holder<Rs...>> replacement()
    {
      return {builder_conversion_tag{}, *this};
    }
  };

  template <typename... Ps, typename... Rs>
  explicit CACHE(Builder<builder_module_type_holder<Ps...>, builder_module_type_holder<Rs...>> b)
      : champsim::operable(b.m_freq_scale), upper_levels(std::move(b.m_uls)), lower_level(b.m_ll), lower_translate(b.m_lt), NAME(b.m_name), NUM_SET(b.m_sets),
        NUM_WAY(b.m_ways), MSHR_SIZE(b.m_mshr_size), PQ_SIZE(b.m_pq_size), HIT_LATENCY((b.m_hit_lat > 0) ? b.m_hit_lat : b.m_latency - b.m_fill_lat),
        FILL_LATENCY(b.m_fill_lat), OFFSET_BITS(b.m_offset_bits), MAX_TAG(b.m_max_tag), MAX_FILL(b.m_max_fill), prefetch_as_load(b.m_pref_load),
        match_offset_bits(b.m_wq_full_addr), virtual_prefetch(b.m_va_pref), pref_activate_mask(b.m_pref_act_mask),
        pref_module_pimpl(std::make_unique<prefetcher_module_model<Ps...>>(this)), repl_module_pimpl(std::make_unique<replacement_module_model<Rs...>>(this))
  {
  }
};

template <typename... Ps>
void CACHE::prefetcher_module_model<Ps...>::impl_prefetcher_initialize()
{
  auto process_one = [&](auto& p) {
    if constexpr (champsim::modules::detect::prefetcher::has_initialize<decltype(p)>())
      p.prefetcher_initialize();
  };

  std::apply([&](auto&... p) { (..., process_one(p)); }, intern_);
}

template <typename... Ps>
uint32_t CACHE::prefetcher_module_model<Ps...>::impl_prefetcher_cache_operate(champsim::address addr, champsim::address ip, bool cache_hit, bool useful_prefetch,
                                                                              access_type type, uint32_t metadata_in)
{
  auto process_one = [&](auto& p) {
    constexpr auto interface_version = champsim::modules::detect::prefetcher::has_cache_operate<decltype(p)>();
    if constexpr (interface_version == 3)
      return p.prefetcher_cache_operate(addr, ip, cache_hit, useful_prefetch, type, metadata_in);
    if constexpr (interface_version == 2)
      return p.prefetcher_cache_operate(addr, ip, cache_hit, useful_prefetch, champsim::to_underlying(type), metadata_in);
    if constexpr (interface_version == 1)
      return p.prefetcher_cache_operate(addr.to<uint64_t>(), ip.to<uint64_t>(), cache_hit, champsim::to_underlying(type), metadata_in); // absent useful_prefetch
    return 0u;
  };

  return std::apply([&](auto&... p) { return (0 ^ ... ^ process_one(p)); }, intern_);
}

template <typename... Ps>
uint32_t CACHE::prefetcher_module_model<Ps...>::impl_prefetcher_cache_fill(champsim::address addr, long set, long way, bool prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  auto process_one = [&](auto& p) {
    constexpr auto interface_version = champsim::modules::detect::prefetcher::has_cache_fill<decltype(p)>();
    if constexpr (interface_version == 1)
      return p.prefetcher_cache_fill(addr.to<uint64_t>(), set, way, prefetch, evicted_addr.to<uint64_t>(), metadata_in);
    if constexpr (interface_version == 2)
      return p.prefetcher_cache_fill(addr, set, way, prefetch, evicted_addr, metadata_in);
    return 0u;
  };

  return std::apply([&](auto&... p) { return (0 ^ ... ^ process_one(p)); }, intern_);
}

template <typename... Ps>
void CACHE::prefetcher_module_model<Ps...>::impl_prefetcher_cycle_operate()
{
  auto process_one = [&](auto& p) {
    if constexpr (champsim::modules::detect::prefetcher::has_cycle_operate<decltype(p)>())
      p.prefetcher_cycle_operate();
  };

  std::apply([&](auto&... p) { (..., process_one(p)); }, intern_);
}

template <typename... Ps>
void CACHE::prefetcher_module_model<Ps...>::impl_prefetcher_final_stats()
{
  auto process_one = [&](auto& p) {
    if constexpr (champsim::modules::detect::prefetcher::has_final_stats<decltype(p)>())
      p.prefetcher_final_stats();
  };

  std::apply([&](auto&... p) { (..., process_one(p)); }, intern_);
}

template <typename... Ps>
void CACHE::prefetcher_module_model<Ps...>::impl_prefetcher_branch_operate(champsim::address ip, uint8_t branch_type, champsim::address branch_target)
{
  auto process_one = [&](auto& p) {
    constexpr auto interface_version = champsim::modules::detect::prefetcher::has_branch_operate<decltype(p)>();
    if constexpr (interface_version == 1)
      p.prefetcher_branch_operate(ip.to<uint64_t>(), branch_type, branch_target.to<uint64_t>());
    else if constexpr (interface_version == 2)
      p.prefetcher_branch_operate(ip, branch_type, branch_target);
  };

  std::apply([&](auto&... p) { (..., process_one(p)); }, intern_);
}

template <typename... Rs>
void CACHE::replacement_module_model<Rs...>::impl_initialize_replacement()
{
  auto process_one = [&](auto& r) {
    if constexpr (champsim::modules::detect::replacement::has_initialize<decltype(r)>())
      r.initialize_replacement();
  };

  std::apply([&](auto&... r) { (..., process_one(r)); }, intern_);
}

template <typename... Rs>
long CACHE::replacement_module_model<Rs...>::impl_find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const BLOCK* current_set, champsim::address ip, champsim::address full_addr,
                                            access_type type)
{
  auto process_one = [&](auto& r) {
    constexpr auto interface_version = champsim::modules::detect::replacement::has_find_victim<decltype(r)>();
    if constexpr (interface_version == 3)
      return r.find_victim(triggering_cpu, instr_id, set, current_set, ip, full_addr, type);
    if constexpr (interface_version == 2)
      return r.find_victim(triggering_cpu, instr_id, set, current_set, ip, full_addr, champsim::to_underlying(type));
    if constexpr (interface_version == 1)
      return r.find_victim(triggering_cpu, instr_id, set, current_set, ip.to<uint64_t>(), full_addr.to<uint64_t>(), champsim::to_underlying(type));
    return 0L;
  };

  return std::apply([&](auto&... r) { return (..., process_one(r)); }, intern_);
}

template <typename... Rs>
void CACHE::replacement_module_model<Rs...>::impl_update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                       access_type type, uint8_t hit)
{
  auto process_one = [&](auto& r) {
    constexpr auto interface_version = champsim::modules::detect::replacement::has_update_state<decltype(r)>();
    if constexpr (interface_version == 3)
      r.update_replacement_state(triggering_cpu, set, way, full_addr, ip, victim_addr, type, hit);
    if constexpr (interface_version == 2)
      r.update_replacement_state(triggering_cpu, set, way, full_addr, ip, victim_addr, champsim::to_underlying(type), hit);
    if constexpr (interface_version == 1)
      r.update_replacement_state(triggering_cpu, set, way, full_addr.to<uint64_t>(), ip.to<uint64_t>(), victim_addr.to<uint64_t>(), champsim::to_underlying(type), hit);
  };

  std::apply([&](auto&... r) { (..., process_one(r)); }, intern_);
}

template <typename... Rs>
void CACHE::replacement_module_model<Rs...>::impl_replacement_final_stats()
{
  auto process_one = [&](auto& r) {
    if constexpr (champsim::modules::detect::replacement::has_final_stats<decltype(r)>())
      r.replacement_final_stats();
  };

  std::apply([&](auto&... r) { (..., process_one(r)); }, intern_);
}
#endif

#ifdef SET_ASIDE_CHAMPSIM_MODULE
#undef SET_ASIDE_CHAMPSIM_MODULE
#define CHAMPSIM_MODULE
#endif
