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

#include <limits>
#include <optional>

#include "util/bits.h"

class CACHE;
namespace champsim
{
class channel;
class cache_builder_conversion_tag
{
};
template <unsigned long long P_FLAG = 0, unsigned long long R_FLAG = 0>
class cache_builder
{
  using self_type = cache_builder<P_FLAG, R_FLAG>;

  std::string m_name{};
  double m_freq_scale{1};
  std::optional<uint32_t> m_sets{};
  double m_sets_factor{64};
  uint32_t m_ways{1};
  std::size_t m_pq_size{std::numeric_limits<std::size_t>::max()};
  std::optional<uint32_t> m_mshr_size{};
  double m_mshr_factor{1};
  std::optional<uint64_t> m_hit_lat{};
  uint64_t m_fill_lat{1};
  uint64_t m_latency{2};
  std::optional<uint32_t> m_max_tag{};
  std::optional<uint32_t> m_max_fill{};
  double m_bandwidth_factor{1};
  unsigned m_offset_bits{};
  bool m_pref_load{};
  bool m_wq_full_addr{};
  bool m_va_pref{};

  unsigned m_pref_act_mask{};
  std::vector<champsim::channel*> m_uls{};
  champsim::channel* m_ll{};
  champsim::channel* m_lt{nullptr};

  friend class ::CACHE;

  template <unsigned long long OTHER_P, unsigned long long OTHER_R>
  friend class cache_builder;

  template <unsigned long long OTHER_P, unsigned long long OTHER_R>
  cache_builder(cache_builder_conversion_tag /*tag*/, const cache_builder<OTHER_P, OTHER_R>& other)
      : m_name(other.m_name), m_freq_scale(other.m_freq_scale), m_sets(other.m_sets), m_sets_factor(other.m_sets_factor), m_ways(other.m_ways),
        m_pq_size(other.m_pq_size), m_mshr_size(other.m_mshr_size), m_mshr_factor(other.m_mshr_factor), m_hit_lat(other.m_hit_lat),
        m_fill_lat(other.m_fill_lat), m_latency(other.m_latency), m_max_tag(other.m_max_tag), m_max_fill(other.m_max_fill),
        m_bandwidth_factor(other.m_bandwidth_factor), m_offset_bits(other.m_offset_bits), m_pref_load(other.m_pref_load), m_wq_full_addr(other.m_wq_full_addr),
        m_va_pref(other.m_va_pref), m_pref_act_mask(other.m_pref_act_mask), m_uls(other.m_uls), m_ll(other.m_ll), m_lt(other.m_lt)
  {
  }

public:
  cache_builder() = default;

  self_type& name(std::string name_);
  self_type& frequency(double freq_scale_);
  self_type& sets(uint32_t sets_);
  self_type& sets_factor(double sets_factor_);
  self_type& ways(uint32_t ways_);
  self_type& pq_size(uint32_t pq_size_);
  self_type& mshr_size(uint32_t mshr_size_);
  self_type& mshr_factor(double mshr_factor_);
  self_type& latency(uint64_t lat_);
  self_type& hit_latency(uint64_t hit_lat_);
  self_type& fill_latency(uint64_t fill_lat_);
  self_type& tag_bandwidth(uint32_t max_read_);
  self_type& fill_bandwidth(uint32_t max_write_);
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
  template <unsigned long long P>
  cache_builder<P, R_FLAG> prefetcher();
  template <unsigned long long R>
  cache_builder<P_FLAG, R> replacement();
};
} // namespace champsim

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::name(std::string name_) -> self_type&
{
  m_name = name_;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::frequency(double freq_scale_) -> self_type&
{
  m_freq_scale = freq_scale_;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::sets(uint32_t sets_) -> self_type&
{
  m_sets = sets_;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::sets_factor(double sets_factor_) -> self_type&
{
  m_sets_factor = sets_factor_;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::ways(uint32_t ways_) -> self_type&
{
  m_ways = ways_;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::pq_size(uint32_t pq_size_) -> self_type&
{
  m_pq_size = pq_size_;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::mshr_size(uint32_t mshr_size_) -> self_type&
{
  m_mshr_size = mshr_size_;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::mshr_factor(double mshr_factor_) -> self_type&
{
  m_mshr_factor = mshr_factor_;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::latency(uint64_t lat_) -> self_type&
{
  m_latency = lat_;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::hit_latency(uint64_t hit_lat_) -> self_type&
{
  m_hit_lat = hit_lat_;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::fill_latency(uint64_t fill_lat_) -> self_type&
{
  m_fill_lat = fill_lat_;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::tag_bandwidth(uint32_t max_read_) -> self_type&
{
  m_max_tag = max_read_;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::fill_bandwidth(uint32_t max_write_) -> self_type&
{
  m_max_fill = max_write_;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::bandwidth_factor(double bandwidth_factor_) -> self_type&
{
  m_bandwidth_factor = bandwidth_factor_;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::offset_bits(unsigned offset_bits_) -> self_type&
{
  m_offset_bits = offset_bits_;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::set_prefetch_as_load() -> self_type&
{
  m_pref_load = true;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::reset_prefetch_as_load() -> self_type&
{
  m_pref_load = false;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::set_wq_checks_full_addr() -> self_type&
{
  m_wq_full_addr = true;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::reset_wq_checks_full_addr() -> self_type&
{
  m_wq_full_addr = false;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::set_virtual_prefetch() -> self_type&
{
  m_va_pref = true;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::reset_virtual_prefetch() -> self_type&
{
  m_va_pref = false;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
template <typename... Elems>
auto champsim::cache_builder<P_FLAG, R_FLAG>::prefetch_activate(Elems... pref_act_elems) -> self_type&
{
  m_pref_act_mask = ((1U << champsim::to_underlying(pref_act_elems)) | ... | 0);
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::upper_levels(std::vector<champsim::channel*>&& uls_) -> self_type&
{
  m_uls = std::move(uls_);
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::lower_level(champsim::channel* ll_) -> self_type&
{
  m_ll = ll_;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
auto champsim::cache_builder<P_FLAG, R_FLAG>::lower_translate(champsim::channel* lt_) -> self_type&
{
  m_lt = lt_;
  return *this;
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
template <unsigned long long P>
champsim::cache_builder<P, R_FLAG> champsim::cache_builder<P_FLAG, R_FLAG>::prefetcher()
{
  return champsim::cache_builder<P, R_FLAG>{cache_builder_conversion_tag{}, *this};
}

template <unsigned long long P_FLAG, unsigned long long R_FLAG>
template <unsigned long long R>
champsim::cache_builder<P_FLAG, R> champsim::cache_builder<P_FLAG, R_FLAG>::replacement()
{
  return champsim::cache_builder<P_FLAG, R>{cache_builder_conversion_tag{}, *this};
}

#endif
