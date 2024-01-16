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

#ifndef PTW_BUILDER_H
#define PTW_BUILDER_H

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "bandwidth.h"
#include "chrono.h"

class VirtualMemory;
class PageTableWalker;
namespace champsim
{
class channel;
class ptw_builder
{
  std::string_view m_name{};
  chrono::picoseconds m_clock_period{250};
  uint32_t m_cpu{0};
  std::array<std::array<uint32_t, 3>, 16> m_pscl{}; // fixed size for now
  std::optional<uint32_t> m_mshr_size{};
  double m_mshr_factor{1};
  std::optional<champsim::bandwidth::maximum_type> m_max_tag_check{};
  std::optional<champsim::bandwidth::maximum_type> m_max_fill{};
  double m_bandwidth_factor{1};
  unsigned m_latency{};
  std::vector<champsim::channel*> m_uls{};
  champsim::channel* m_ll{};
  VirtualMemory* m_vmem{};

  friend class ::PageTableWalker;

  uint32_t scaled_by_ul_size(double factor) const;

public:
  ptw_builder& name(std::string_view name_);
  ptw_builder& clock_period(champsim::chrono::picoseconds clock_period_);
  ptw_builder& cpu(uint32_t cpu_);
  ptw_builder& add_pscl(uint8_t lvl, uint32_t set, uint32_t way);
  ptw_builder& mshr_size(uint32_t mshr_size_);
  ptw_builder& mshr_factor(double mshr_factor_);
  ptw_builder& tag_bandwidth(champsim::bandwidth::maximum_type max_read_);
  ptw_builder& fill_bandwidth(champsim::bandwidth::maximum_type max_fill_);
  ptw_builder& bandwidth_factor(double bandwidth_factor_);
  ptw_builder& latency(unsigned latency_);
  ptw_builder& upper_levels(std::vector<champsim::channel*>&& uls_);
  ptw_builder& lower_level(champsim::channel* ll_);
  ptw_builder& virtual_memory(VirtualMemory* vmem_);
};
} // namespace champsim

#endif
