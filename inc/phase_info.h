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

#ifndef PHASE_INFO_H
#define PHASE_INFO_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "cache.h"
#include "dram_controller.h"
#include "ooo_cpu.h"
#include <string_view>

namespace champsim
{

struct phase_info {
  std::string name;
  bool is_warmup;
  uint64_t length;
  std::vector<std::size_t> trace_index;
  std::vector<std::string> trace_names;
};

struct phase_stats {
  std::string name;
  std::vector<std::string> trace_names;
  std::vector<O3_CPU::stats_type> roi_cpu_stats, sim_cpu_stats;
  std::vector<CACHE::stats_type> roi_cache_stats, sim_cache_stats;
  std::vector<DRAM_CHANNEL::stats_type> roi_dram_stats, sim_dram_stats;
};

} // namespace champsim

#endif
