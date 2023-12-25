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

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

#include "champsim_constants.h"
#include "channel.h"

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
  double m_mshr_factor{1};
  std::optional<uint64_t> m_hit_lat{};
  uint64_t m_fill_lat{1};
  uint64_t m_latency{2};
  std::optional<champsim::bandwidth::maximum_type> m_max_tag{};
  std::optional<champsim::bandwidth::maximum_type> m_max_fill{};
  double m_bandwidth_factor{1};
  unsigned m_offset_bits{};
  bool m_pref_load{};
  bool m_wq_full_addr{};
  bool m_va_pref{};

  std::vector<access_type> m_pref_act_mask{access_type::LOAD, access_type::PREFETCH};
  std::vector<champsim::channel*> m_uls{};
  champsim::channel* m_ll{};
  champsim::channel* m_lt{nullptr};
};
} // namespace detail

template <typename P = cache_builder_module_type_holder<>, typename R = cache_builder_module_type_holder<>, typename S = cache_builder_module_type_holder<>>
class cache_builder : public detail::cache_builder_base
{
  using self_type = cache_builder<P, R, S>;

  friend class ::CACHE;

  template <typename OTHER_P, typename OTHER_R, typename OTHER_S>
  friend class cache_builder;

  explicit cache_builder(const detail::cache_builder_base& other) : detail::cache_builder_base(other) {}

  uint32_t scaled_by_ul_size(double factor) const { return factor < 0 ? 0 : static_cast<uint32_t>(std::lround(factor * std::floor(std::size(m_uls)))); }

  uint32_t get_num_sets() const;
  uint32_t get_num_ways() const;
  uint32_t get_num_mshrs() const;
  champsim::bandwidth::maximum_type get_tag_bandwidth() const;
  champsim::bandwidth::maximum_type get_fill_bandwidth() const;

public:
  cache_builder() = default;

  self_type& name(std::string name_);
  self_type& frequency(double freq_scale_);
  self_type& size(uint64_t size_);
  self_type& sets(uint32_t sets_);
  self_type& sets_factor(double sets_factor_);
  self_type& ways(uint32_t ways_);
  self_type& pq_size(uint32_t pq_size_);
  self_type& mshr_size(uint32_t mshr_size_);
  self_type& mshr_factor(double mshr_factor_);
  self_type& latency(uint64_t lat_);
  self_type& hit_latency(uint64_t hit_lat_);
  self_type& fill_latency(uint64_t fill_lat_);
  self_type& tag_bandwidth(champsim::bandwidth::maximum_type max_read_);
  self_type& fill_bandwidth(champsim::bandwidth::maximum_type max_write_);
  self_type& bandwidth_factor(double bandwidth_factor_);
  self_type& offset_bits(unsigned offset_bits_);
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
  cache_builder<cache_builder_module_type_holder<Ps...>, R, S> prefetcher();
  template <typename... Rs>
  cache_builder<P, cache_builder_module_type_holder<Rs...>, S> replacement();
  template <typename... Ss>
  cache_builder<P, R, cache_builder_module_type_holder<Ss...>> state_model();
};
} // namespace champsim

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::get_num_sets() const -> uint32_t
{
  if (m_sets.has_value())
    return m_sets.value();
  if (m_size.has_value() && m_ways.has_value())
    return static_cast<uint32_t>(m_size.value() / (m_ways.value() * BLOCK_SIZE)); // casting the result of division
  return scaled_by_ul_size(m_sets_factor);
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::get_num_ways() const -> uint32_t
{
  if (m_ways.has_value())
    return m_ways.value();
  if (m_size.has_value())
    return static_cast<uint32_t>(m_size.value() / (get_num_sets() * BLOCK_SIZE)); // casting the result of division
  return 1;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::get_num_mshrs() const -> uint32_t
{
  return m_mshr_size.value_or(scaled_by_ul_size(m_mshr_factor));
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::get_tag_bandwidth() const -> champsim::bandwidth::maximum_type
{
  return m_max_tag.value_or(champsim::bandwidth::maximum_type{scaled_by_ul_size(m_bandwidth_factor)});
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::get_fill_bandwidth() const -> champsim::bandwidth::maximum_type
{
  return m_max_fill.value_or(champsim::bandwidth::maximum_type{scaled_by_ul_size(m_bandwidth_factor)});
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::name(std::string name_) -> self_type&
{
  m_name = name_;
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::frequency(double freq_scale_) -> self_type&
{
  m_freq_scale = freq_scale_;
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::size(uint64_t size_) -> self_type&
{
  m_size = size_;
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::sets(uint32_t sets_) -> self_type&
{
  m_sets = sets_;
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::sets_factor(double sets_factor_) -> self_type&
{
  m_sets_factor = sets_factor_;
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::ways(uint32_t ways_) -> self_type&
{
  m_ways = ways_;
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::pq_size(uint32_t pq_size_) -> self_type&
{
  m_pq_size = pq_size_;
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::mshr_size(uint32_t mshr_size_) -> self_type&
{
  m_mshr_size = mshr_size_;
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::mshr_factor(double mshr_factor_) -> self_type&
{
  m_mshr_factor = mshr_factor_;
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::latency(uint64_t lat_) -> self_type&
{
  m_latency = lat_;
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::hit_latency(uint64_t hit_lat_) -> self_type&
{
  m_hit_lat = hit_lat_;
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::fill_latency(uint64_t fill_lat_) -> self_type&
{
  m_fill_lat = fill_lat_;
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::tag_bandwidth(champsim::bandwidth::maximum_type max_read_) -> self_type&
{
  m_max_tag = max_read_;
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::fill_bandwidth(champsim::bandwidth::maximum_type max_write_) -> self_type&
{
  m_max_fill = max_write_;
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::bandwidth_factor(double bandwidth_factor_) -> self_type&
{
  m_bandwidth_factor = bandwidth_factor_;
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::offset_bits(unsigned offset_bits_) -> self_type&
{
  m_offset_bits = offset_bits_;
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::set_prefetch_as_load() -> self_type&
{
  m_pref_load = true;
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::reset_prefetch_as_load() -> self_type&
{
  m_pref_load = false;
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::set_wq_checks_full_addr() -> self_type&
{
  m_wq_full_addr = true;
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::reset_wq_checks_full_addr() -> self_type&
{
  m_wq_full_addr = false;
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::set_virtual_prefetch() -> self_type&
{
  m_va_pref = true;
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::reset_virtual_prefetch() -> self_type&
{
  m_va_pref = false;
  return *this;
}

template <typename P, typename R, typename S>
template <typename... Elems>
auto champsim::cache_builder<P, R, S>::prefetch_activate(Elems... pref_act_elems) -> self_type&
{
  m_pref_act_mask = {pref_act_elems...};
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::upper_levels(std::vector<champsim::channel*>&& uls_) -> self_type&
{
  m_uls = std::move(uls_);
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::lower_level(champsim::channel* ll_) -> self_type&
{
  m_ll = ll_;
  return *this;
}

template <typename P, typename R, typename S>
auto champsim::cache_builder<P, R, S>::lower_translate(champsim::channel* lt_) -> self_type&
{
  m_lt = lt_;
  return *this;
}

template <typename P, typename R, typename S>
template <typename... Ps>
auto champsim::cache_builder<P, R, S>::prefetcher() -> champsim::cache_builder<champsim::cache_builder_module_type_holder<Ps...>, R, S>
{
  return champsim::cache_builder<champsim::cache_builder_module_type_holder<Ps...>, R, S>{*this};
}

template <typename P, typename R, typename S>
template <typename... Rs>
auto champsim::cache_builder<P, R, S>::replacement() -> champsim::cache_builder<P, champsim::cache_builder_module_type_holder<Rs...>, S>
{
  return champsim::cache_builder<P, champsim::cache_builder_module_type_holder<Rs...>, S>{*this};
}

template <typename P, typename R, typename S>
template <typename... Ss>
auto champsim::cache_builder<P, R, S>::state_model() -> champsim::cache_builder<P, R, champsim::cache_builder_module_type_holder<Ss...>>
{
  return champsim::cache_builder<P, R, champsim::cache_builder_module_type_holder<Ss...>>{*this};
}

#endif
