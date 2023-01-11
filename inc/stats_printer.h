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

#include <iostream>
#include <vector>

#include "cache.h"
#include "dram_controller.h"
#include "ooo_cpu.h"

namespace champsim
{
struct phase_stats {
  std::string name;
  std::vector<std::string> trace_names;
  std::vector<O3_CPU::stats_type> roi_cpu_stats, sim_cpu_stats;
  std::vector<CACHE::stats_type> roi_cache_stats, sim_cache_stats;
  std::vector<DRAM_CHANNEL::stats_type> roi_dram_stats, sim_dram_stats;
};

class plain_printer
{
  std::ostream& stream;

  void print(O3_CPU::stats_type);
  void print(CACHE::stats_type);
  void print(DRAM_CHANNEL::stats_type);

  template <typename T>
  void print(std::vector<T> stats_list)
  {
    for (auto& stats : stats_list)
      print(stats);
  }

public:
  plain_printer(std::ostream& str) : stream(str) {}
  void print(phase_stats& stats);
  void print(std::vector<phase_stats>& stats);
};

class json_printer
{
  std::ostream& stream;

  void print(O3_CPU::stats_type);
  void print(CACHE::stats_type);
  void print(DRAM_CHANNEL::stats_type);

  std::size_t indent_level = 0;
  std::string indent() const { return std::string(2 * indent_level, ' '); }

  void print(std::vector<O3_CPU::stats_type> stats_list);
  void print(std::vector<CACHE::stats_type> stats_list);
  void print(std::vector<DRAM_CHANNEL::stats_type> stats_list);

public:
  json_printer(std::ostream& str) : stream(str) {}
  void print(phase_stats& stats);
  void print(std::vector<phase_stats>& stats);
};
} // namespace champsim
