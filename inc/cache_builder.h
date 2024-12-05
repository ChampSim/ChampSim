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
#ifndef CACHE_BUILDER_H
#define CACHE_BUILDER_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

#include "champsim.h"
#include "channel.h"
#include "chrono.h"
#include "util/bits.h"
#include "util/to_underlying.h"
#include "bandwidth.h"

class CACHE;
namespace champsim
{
class channel;
namespace detail
{
struct cache_builder_base {
  std::string m_name{};
  chrono::picoseconds m_clock_period{250};
  std::optional<champsim::data::bytes> m_size{};
  std::optional<uint32_t> m_sets{};
  double m_sets_factor{64};
  std::optional<uint32_t> m_ways{};
  std::size_t m_pq_size{std::numeric_limits<std::size_t>::max()};
  std::optional<uint32_t> m_mshr_size{};
  std::optional<uint64_t> m_hit_lat{};
  std::optional<uint64_t> m_fill_lat{};
  std::optional<uint64_t> m_latency{};
  std::optional<champsim::bandwidth::maximum_type> m_max_tag{};
  std::optional<champsim::bandwidth::maximum_type> m_max_fill{};
  champsim::data::bits m_offset_bits{LOG2_BLOCK_SIZE};
  bool m_pref_load{};
  bool m_wq_full_addr{};
  bool m_va_pref{};

  std::vector<access_type> m_pref_act_mask{access_type::LOAD, access_type::PREFETCH};
  std::vector<champsim::channel*> m_uls{};
  champsim::channel* m_ll{};
  champsim::channel* m_lt{nullptr};

  std::vector<std::string> m_pref_modules{};
  std::vector<std::string> m_repl_modules{};
};
} // namespace detail

class cache_builder : public detail::cache_builder_base
{
  using self_type = cache_builder;

  friend class ::CACHE;

  explicit cache_builder(const detail::cache_builder_base& other) : detail::cache_builder_base(other) {}

  uint32_t scaled_by_ul_size(double factor) const { return factor < 0 ? 0 : static_cast<uint32_t>(std::lround(factor * std::floor(std::size(m_uls)))); }

  uint32_t get_num_sets() const;
  uint32_t get_num_ways() const;
  uint32_t get_num_mshrs() const;
  champsim::bandwidth::maximum_type get_tag_bandwidth() const;
  champsim::bandwidth::maximum_type get_fill_bandwidth() const;
  uint64_t get_hit_latency() const;
  uint64_t get_fill_latency() const;
  uint64_t get_total_latency() const;

public:
  cache_builder() = default;

  /**
   * Specify the name of the cache.
   * This will be a unique identifier in the statistics.
   */
  self_type& name(std::string name_);

  /**
   * Specify the clock period of the cache.
   */
  self_type& clock_period(champsim::chrono::picoseconds clock_period_);

  /**
   * Specify the size of the cache.
   *
   * If the number of sets or ways is not specified, this value can be used to derive them.
   */
  self_type& size(champsim::data::bytes size_);

  /**
   * Specify the logarithm of the cache size.
   */
  self_type& log2_size(uint64_t log2_size_);

  /**
   * Specify the number of sets in the cache.
   */
  self_type& sets(uint32_t sets_);

  /**
   * Specify the logarithm of the number of sets in the cache.
   */
  self_type& log2_sets(uint32_t log2_sets_);
  self_type& sets_factor(double sets_factor_);

  /**
   * Specify the number of ways in the cache.
   */
  self_type& ways(uint32_t ways_);

  /**
   * Specify the logarithm of the number of ways in the cache.
   */
  self_type& log2_ways(uint32_t log2_ways_);

  /**
   * Specify the size of the internal prefetch queue.
   */
  self_type& pq_size(uint32_t pq_size_);

  /**
   * Specify the number of MSHRs.
   * If this is not specified, it will be derived from the number of sets, fill latency, and fill bandwidth.
   */
  self_type& mshr_size(uint32_t mshr_size_);

  /**
   * Specify the latency of the cache, in cycles.
   * If the hit latency and fill latency are not specified, this will be distributed evenly between them.
   */
  self_type& latency(uint64_t lat_);

  /**
   * Specify the latency of the cache's tag check, in cycles.
   */
  self_type& hit_latency(uint64_t hit_lat_);

  /**
   * Specify the latency of the cache fill operation, in cycles.
   */
  self_type& fill_latency(uint64_t fill_lat_);

  /**
   * Specify the bandwidth of the cache's tag check.
   */
  self_type& tag_bandwidth(champsim::bandwidth::maximum_type max_read_);

  /**
   * Specify the bandwidth of the cache fill.
   */
  self_type& fill_bandwidth(champsim::bandwidth::maximum_type max_write_);

  /**
   * Specify the number of bits to be used as a block offset.
   */
  self_type& offset_bits(champsim::data::bits offset_bits_);

  /**
   * Specify the logarithm of the number of bits to be used as a block offset.
   */
  self_type& log2_offset_bits(unsigned log2_offset_bits_);

  /**
   * Specify that prefetches should be issued with the same priority as loads.
   */
  self_type& set_prefetch_as_load();

  /**
   * Specify that prefetches should be issued with lower priority than loads.
   */
  self_type& reset_prefetch_as_load();

  /**
   * Specify that the write queue should check the full address (including the offset) when determining collisions.
   */
  self_type& set_wq_checks_full_addr();

  /**
   * Specify that the write queue should ignore the offset bits when determining collisions.
   */
  self_type& reset_wq_checks_full_addr();

  /**
   * Specify that prefetchers should operate in the virtual address space.
   */
  self_type& set_virtual_prefetch();

  /**
   * Specify that prefetchers should operate in the physical address space.
   */
  self_type& reset_virtual_prefetch();

  /**
   * Specify the ``access_type`` values that should activate the prefetcher.
   */
  template <typename... Elems>
  self_type& prefetch_activate(Elems... pref_act_elems);

  /**
   * Specify the upper levels to this cache.
   */
  self_type& upper_levels(std::vector<champsim::channel*>&& uls_);

  /**
   * Specify the lower level of the cache.
   */
  self_type& lower_level(champsim::channel* ll_);

  /**
   * Specify the translator (TLB) for this cache.
   */
  self_type& lower_translate(champsim::channel* lt_);

  template <typename... Elems>
  self_type& prefetcher(Elems... pref_modules);

  template <typename... Elems>
  self_type& replacement(Elems... repl_modules);
};
} // namespace champsim

template <typename... Elems>
auto champsim::cache_builder::prefetch_activate(Elems... pref_act_elems) -> self_type&
{
  m_pref_act_mask = {pref_act_elems...};
  return *this;
}

template <typename... Elems>
auto champsim::cache_builder::prefetcher(Elems... pref_modules) -> self_type&
{
  m_pref_modules = {pref_modules...};
  return *this;
}

template <typename... Elems>
auto champsim::cache_builder::replacement(Elems... repl_modules) -> self_type&
{
  m_repl_modules = {repl_modules...};
  return *this;
}

#endif
