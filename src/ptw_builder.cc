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

#include "ptw_builder.h"

#include <cmath>
#include <utility>

auto champsim::ptw_builder::name(std::string_view name_) -> ptw_builder&
{
  m_name = name_;
  return *this;
}

auto champsim::ptw_builder::clock_period(champsim::chrono::picoseconds clock_period_) -> ptw_builder&
{
  m_clock_period = clock_period_;
  return *this;
}

auto champsim::ptw_builder::cpu(uint32_t cpu_) -> ptw_builder&
{
  m_cpu = cpu_;
  return *this;
}

auto champsim::ptw_builder::add_pscl(uint8_t lvl, uint32_t set, uint32_t way) -> ptw_builder&
{
  m_pscl.at(lvl) = {lvl, set, way};
  return *this;
}

auto champsim::ptw_builder::mshr_size(uint32_t mshr_size_) -> ptw_builder&
{
  m_mshr_size = mshr_size_;
  return *this;
}

auto champsim::ptw_builder::mshr_factor(double mshr_factor_) -> ptw_builder&
{
  m_mshr_factor = mshr_factor_;
  return *this;
}

auto champsim::ptw_builder::tag_bandwidth(champsim::bandwidth::maximum_type max_read_) -> ptw_builder&
{
  m_max_tag_check = max_read_;
  return *this;
}

auto champsim::ptw_builder::fill_bandwidth(champsim::bandwidth::maximum_type max_fill_) -> ptw_builder&
{
  m_max_fill = max_fill_;
  return *this;
}

auto champsim::ptw_builder::bandwidth_factor(double bandwidth_factor_) -> ptw_builder&
{
  m_bandwidth_factor = bandwidth_factor_;
  return *this;
}

auto champsim::ptw_builder::latency(unsigned latency_) -> ptw_builder&
{
  m_latency = latency_;
  return *this;
}

auto champsim::ptw_builder::upper_levels(std::vector<champsim::channel*>&& uls_) -> ptw_builder&
{
  m_uls = std::move(uls_);
  return *this;
}

auto champsim::ptw_builder::lower_level(champsim::channel* ll_) -> ptw_builder&
{
  m_ll = ll_;
  return *this;
}

auto champsim::ptw_builder::virtual_memory(VirtualMemory* vmem_) -> ptw_builder&
{
  m_vmem = vmem_;
  return *this;
}

auto champsim::ptw_builder::scaled_by_ul_size(double factor) const -> uint32_t
{
  return factor < 0 ? 0 : static_cast<uint32_t>(std::lround(factor * std::floor(std::size(m_uls))));
}
