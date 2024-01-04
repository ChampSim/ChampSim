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
#include "util/bits.h"
#include "util/to_underlying.h"

class CACHE;
namespace champsim
{
class channel;
template <typename... Ts>
class cache_builder_module_type_holder
{
};
namespace detail
{
struct cache_builder_base {
  std::string m_name{};
  double m_freq_scale{1};
  std::optional<uint64_t> m_size{};
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
  unsigned m_offset_bits{LOG2_BLOCK_SIZE};
  bool m_pref_load{};
  bool m_wq_full_addr{};
  bool m_va_pref{};

  std::vector<access_type> m_pref_act_mask{access_type::LOAD, access_type::PREFETCH};
  std::vector<champsim::channel*> m_uls{};
  champsim::channel* m_ll{};
  champsim::channel* m_lt{nullptr};
};
} // namespace detail

template <typename P = cache_builder_module_type_holder<>, typename R = cache_builder_module_type_holder<>>
class cache_builder : public detail::cache_builder_base
{
  using self_type = cache_builder<P, R>;

  friend class ::CACHE;

  template <typename OTHER_P, typename OTHER_R>
  friend class cache_builder;

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

  self_type& name(std::string name_);
  self_type& frequency(double freq_scale_);
  self_type& size(uint64_t size_);
  self_type& log2_size(uint64_t log2_size_);
  self_type& sets(uint32_t sets_);
  self_type& log2_sets(uint32_t log2_sets_);
  self_type& sets_factor(double sets_factor_);
  self_type& ways(uint32_t ways_);
  self_type& log2_ways(uint32_t log2_ways_);
  self_type& pq_size(uint32_t pq_size_);
  self_type& mshr_size(uint32_t mshr_size_);
  self_type& latency(uint64_t lat_);
  self_type& hit_latency(uint64_t hit_lat_);
  self_type& fill_latency(uint64_t fill_lat_);
  self_type& tag_bandwidth(champsim::bandwidth::maximum_type max_read_);
  self_type& fill_bandwidth(champsim::bandwidth::maximum_type max_write_);
  self_type& offset_bits(unsigned offset_bits_);
  self_type& log2_offset_bits(unsigned log2_offset_bits_);
  self_type& set_prefetch_as_load();
  self_type& reset_prefetch_as_load();
  self_type& set_wq_checks_full_addr();
  self_type& reset_wq_checks_full_addr();
  self_type& set_virtual_prefetch();
  self_type& reset_virtual_prefetch();
  template <typename... Elems>
  self_type& prefetch_activate(Elems... pref_act_elems);
  self_type& upper_levels(std::vector<champsim::channel*>&& uls_);
  self_type& lower_level(champsim::channel* ll_);
  self_type& lower_translate(champsim::channel* lt_);
  template <typename... Ps>
  cache_builder<cache_builder_module_type_holder<Ps...>, R> prefetcher();
  template <typename... Rs>
  cache_builder<P, cache_builder_module_type_holder<Rs...>> replacement();
};
} // namespace champsim

template <typename P, typename R>
auto champsim::cache_builder<P, R>::get_num_sets() const -> uint32_t
{
  uint32_t value = 0;
  if (m_sets.has_value())
    value = m_sets.value();
  else if (m_size.has_value() && m_ways.has_value())
    value = static_cast<uint32_t>(m_size.value() / (m_ways.value() * (1 << m_offset_bits))); // casting the result of division
  else
    value = scaled_by_ul_size(m_sets_factor);
  return champsim::next_pow2(value);
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::get_num_ways() const -> uint32_t
{
  if (m_ways.has_value())
    return m_ways.value();
  if (m_size.has_value())
    return static_cast<uint32_t>(m_size.value() / (get_num_sets() * (1 << m_offset_bits))); // casting the result of division
  return 1;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::get_num_mshrs() const -> uint32_t
{
  auto default_count = (get_num_sets() * get_fill_latency() * static_cast<unsigned long>(champsim::to_underlying(get_fill_bandwidth())))
                       >> 4; // fill bandwidth should not be greater than 2^63
  return std::max(m_mshr_size.value_or(default_count), 1u);
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::get_tag_bandwidth() const -> champsim::bandwidth::maximum_type
{
  return std::max(m_max_tag.value_or(champsim::bandwidth::maximum_type{get_num_sets() >> 9}), champsim::bandwidth::maximum_type{1});
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::get_fill_bandwidth() const -> champsim::bandwidth::maximum_type
{
  return m_max_fill.value_or(get_tag_bandwidth());
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::get_hit_latency() const -> uint64_t
{
  if (m_hit_lat.has_value())
    return m_hit_lat.value();
  return get_total_latency() - get_fill_latency();
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::get_fill_latency() const -> uint64_t
{
  if (m_fill_lat.has_value())
    return m_fill_lat.value();
  return (get_total_latency() + 1) / 2;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::get_total_latency() const -> uint64_t
{
  uint64_t latency{0};
  if (m_latency.has_value()) {
    latency = m_latency.value();
  } else {
    auto log_size = champsim::lg2(get_num_sets() * get_num_ways());
    if (log_size > 6) {
      latency = 2 * (log_size - 6);
    } else {
      latency = 1;
    }
  }
  return std::max(latency, uint64_t{1});
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::name(std::string name_) -> self_type&
{
  m_name = name_;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::frequency(double freq_scale_) -> self_type&
{
  m_freq_scale = freq_scale_;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::size(uint64_t size_) -> self_type&
{
  m_size = size_;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::log2_size(uint64_t log2_size_) -> self_type&
{
  m_size = 1 << log2_size_;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::sets(uint32_t sets_) -> self_type&
{
  m_sets = sets_;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::log2_sets(uint32_t log2_sets_) -> self_type&
{
  m_sets = 1 << log2_sets_;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::sets_factor(double sets_factor_) -> self_type&
{
  m_sets_factor = sets_factor_;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::ways(uint32_t ways_) -> self_type&
{
  m_ways = ways_;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::log2_ways(uint32_t log2_ways_) -> self_type&
{
  m_ways = 1 << log2_ways_;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::pq_size(uint32_t pq_size_) -> self_type&
{
  m_pq_size = pq_size_;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::mshr_size(uint32_t mshr_size_) -> self_type&
{
  m_mshr_size = mshr_size_;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::latency(uint64_t lat_) -> self_type&
{
  m_latency = lat_;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::hit_latency(uint64_t hit_lat_) -> self_type&
{
  m_hit_lat = hit_lat_;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::fill_latency(uint64_t fill_lat_) -> self_type&
{
  m_fill_lat = fill_lat_;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::tag_bandwidth(champsim::bandwidth::maximum_type max_read_) -> self_type&
{
  m_max_tag = max_read_;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::fill_bandwidth(champsim::bandwidth::maximum_type max_write_) -> self_type&
{
  m_max_fill = max_write_;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::offset_bits(unsigned offset_bits_) -> self_type&
{
  m_offset_bits = offset_bits_;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::log2_offset_bits(unsigned log2_offset_bits_) -> self_type&
{
  m_offset_bits = 1 << log2_offset_bits_;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::set_prefetch_as_load() -> self_type&
{
  m_pref_load = true;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::reset_prefetch_as_load() -> self_type&
{
  m_pref_load = false;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::set_wq_checks_full_addr() -> self_type&
{
  m_wq_full_addr = true;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::reset_wq_checks_full_addr() -> self_type&
{
  m_wq_full_addr = false;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::set_virtual_prefetch() -> self_type&
{
  m_va_pref = true;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::reset_virtual_prefetch() -> self_type&
{
  m_va_pref = false;
  return *this;
}

template <typename P, typename R>
template <typename... Elems>
auto champsim::cache_builder<P, R>::prefetch_activate(Elems... pref_act_elems) -> self_type&
{
  m_pref_act_mask = {pref_act_elems...};
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::upper_levels(std::vector<champsim::channel*>&& uls_) -> self_type&
{
  m_uls = std::move(uls_);
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::lower_level(champsim::channel* ll_) -> self_type&
{
  m_ll = ll_;
  return *this;
}

template <typename P, typename R>
auto champsim::cache_builder<P, R>::lower_translate(champsim::channel* lt_) -> self_type&
{
  m_lt = lt_;
  return *this;
}

template <typename P, typename R>
template <typename... Ps>
auto champsim::cache_builder<P, R>::prefetcher() -> champsim::cache_builder<champsim::cache_builder_module_type_holder<Ps...>, R>
{
  return champsim::cache_builder<champsim::cache_builder_module_type_holder<Ps...>, R>{*this};
}

template <typename P, typename R>
template <typename... Rs>
auto champsim::cache_builder<P, R>::replacement() -> champsim::cache_builder<P, champsim::cache_builder_module_type_holder<Rs...>>
{
  return champsim::cache_builder<P, champsim::cache_builder_module_type_holder<Rs...>>{*this};
}

#endif
