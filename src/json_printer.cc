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

#include <iomanip>
#include <iostream>
#include <numeric>
#include <utility>

#include "stats_printer.h"

void champsim::json_printer::print(O3_CPU::stats_type stats)
{
  constexpr std::array<std::pair<std::string_view, std::size_t>, 6> types{
      {std::pair{"BRANCH_DIRECT_JUMP", BRANCH_DIRECT_JUMP}, std::pair{"BRANCH_INDIRECT", BRANCH_INDIRECT}, std::pair{"BRANCH_CONDITIONAL", BRANCH_CONDITIONAL},
       std::pair{"BRANCH_DIRECT_CALL", BRANCH_DIRECT_CALL}, std::pair{"BRANCH_INDIRECT_CALL", BRANCH_INDIRECT_CALL},
       std::pair{"BRANCH_RETURN", BRANCH_RETURN}}};

  auto total_mispredictions = std::ceil(
      std::accumulate(std::begin(types), std::end(types), 0ll, [btm = stats.branch_type_misses](auto acc, auto next) { return acc + btm[next.second]; }));

  stream << indent() << "{" << std::endl;
  ++indent_level;
  stream << indent() << "\"instructions\": " << stats.instrs() << "," << std::endl;
  stream << indent() << "\"cycles\": " << stats.cycles() << "," << std::endl;
  stream << indent() << "\"Avg ROB occupancy at mispredict\": " << std::ceil(stats.total_rob_occupancy_at_branch_mispredict) / std::ceil(total_mispredictions)
         << ", " << std::endl;

  stream << indent() << "\"mispredict\": {" << std::endl;
  ++indent_level;
  for (std::size_t i = 0; i < std::size(types); ++i) {
    if (i != 0)
      stream << "," << std::endl;
    stream << indent() << "\"" << types[i].first << "\": " << stats.branch_type_misses[types[i].second];
  }
  stream << std::endl;
  --indent_level;
  stream << indent() << "}" << std::endl;
  --indent_level;
  stream << indent() << "}";
}

void champsim::json_printer::print(CACHE::stats_type stats)
{
  constexpr std::array<std::pair<std::string_view, std::size_t>, 5> types{
      {std::pair{"LOAD", LOAD}, std::pair{"RFO", RFO}, std::pair{"PREFETCH", PREFETCH}, std::pair{"WRITE", WRITE}, std::pair{"TRANSLATION", TRANSLATION}}};

  stream << indent() << "\"" << stats.name << "\": {" << std::endl;
  ++indent_level;
  for (const auto& type : types) {
    stream << indent() << "\"" << type.first << "\": {" << std::endl;
    ++indent_level;

    stream << indent() << "\"hit\": [";
    for (std::size_t i = 0; i < std::size(stats.hits[type.second]); ++i) {
      if (i != 0)
        stream << ", ";
      stream << stats.hits[type.second][i];
    }
    stream << "]," << std::endl;

    stream << indent() << "\"miss\": [";
    for (std::size_t i = 0; i < std::size(stats.misses[type.second]); ++i) {
      if (i != 0)
        stream << ", ";
      stream << stats.misses[type.second][i];
    }
    stream << "]" << std::endl;

    --indent_level;
    stream << indent() << "}," << std::endl;
  }

  stream << indent() << "\"prefetch requested\": " << stats.pf_requested << "," << std::endl;
  stream << indent() << "\"prefetch issued\": " << stats.pf_issued << "," << std::endl;
  stream << indent() << "\"useful prefetch\": " << stats.pf_useful << "," << std::endl;
  stream << indent() << "\"useless prefetch\": " << stats.pf_useless << "," << std::endl;

  double TOTAL_MISS = 0;
  for (const auto& type : types)
    TOTAL_MISS += std::accumulate(std::begin(stats.misses.at(type.second)), std::end(stats.misses.at(type.second)), TOTAL_MISS);
  if (TOTAL_MISS > 0)
    stream << indent() << "\"miss latency\": " << (std::ceil(stats.total_miss_latency)) / TOTAL_MISS << std::endl;
  else
    stream << indent() << "\"miss latency\": null" << std::endl;
  --indent_level;
  stream << indent() << "}";
}

void champsim::json_printer::print(DRAM_CHANNEL::stats_type stats)
{
  stream << indent() << "{" << std::endl;
  ++indent_level;
  stream << indent() << "\"RQ ROW_BUFFER_HIT\": " << stats.RQ_ROW_BUFFER_HIT << "," << std::endl;
  stream << indent() << "\"RQ ROW_BUFFER_MISS\": " << stats.RQ_ROW_BUFFER_MISS << "," << std::endl;
  stream << indent() << "\"WQ ROW_BUFFER_HIT\": " << stats.WQ_ROW_BUFFER_HIT << "," << std::endl;
  stream << indent() << "\"WQ ROW_BUFFER_MISS\": " << stats.WQ_ROW_BUFFER_MISS << "," << std::endl;
  stream << indent() << "\"WQ FULL\": " << stats.WQ_FULL << "," << std::endl;
  if (stats.dbus_count_congested > 0)
    stream << indent() << "\"AVG DBUS CONGESTED CYCLE\": " << std::ceil(stats.dbus_cycle_congested) / std::ceil(stats.dbus_count_congested) << std::endl;
  else
    stream << indent() << "\"AVG DBUS CONGESTED CYCLE\": null" << std::endl;
  --indent_level;
  stream << indent() << "}";
}

void champsim::json_printer::print(std::vector<O3_CPU::stats_type> stats_list)
{
  stream << indent() << "\"cores\": [" << std::endl;
  ++indent_level;

  bool first = true;
  for (const auto& stats : stats_list) {
    if (!first)
      stream << "," << std::endl;
    print(stats);
    first = false;
  }
  stream << std::endl;

  --indent_level;
  stream << indent() << "]";
}

void champsim::json_printer::print(std::vector<CACHE::stats_type> stats_list)
{
  bool first = true;
  for (const auto& stats : stats_list) {
    if (!first)
      stream << "," << std::endl;
    print(stats);
    first = false;
  }
}

void champsim::json_printer::print(std::vector<DRAM_CHANNEL::stats_type> stats_list)
{
  stream << indent() << "\"DRAM\": [" << std::endl;
  ++indent_level;

  bool first = true;
  for (const auto& stats : stats_list) {
    if (!first)
      stream << "," << std::endl;
    print(stats);
    first = false;
  }
  stream << std::endl;

  --indent_level;
  stream << indent() << "]" << std::endl;
}

void champsim::json_printer::print(champsim::phase_stats& stats)
{
  stream << indent() << "{" << std::endl;
  ++indent_level;

  stream << indent() << "\"name\": \"" << stats.name << "\"," << std::endl;
  stream << indent() << "\"traces\": [" << std::endl;
  ++indent_level;

  bool first = true;
  for (auto t : stats.trace_names) {
    if (!first)
      stream << "," << std::endl;
    stream << indent() << "\"" << t << "\"";
    first = false;
  }

  --indent_level;
  stream << std::endl << indent() << "]," << std::endl;

  stream << indent() << "\"roi\": {" << std::endl;
  ++indent_level;

  print(stats.roi_cpu_stats);
  stream << "," << std::endl;

  print(stats.roi_cache_stats);
  stream << "," << std::endl;

  print(stats.roi_dram_stats);
  stream << std::endl;

  --indent_level;
  stream << indent() << "}," << std::endl;

  stream << indent() << "\"sim\": {" << std::endl;
  ++indent_level;

  print(stats.sim_cpu_stats);
  stream << "," << std::endl;

  print(stats.sim_cache_stats);
  stream << "," << std::endl;

  print(stats.sim_dram_stats);
  stream << std::endl;

  --indent_level;
  stream << indent() << "}" << std::endl;
  --indent_level;
  stream << indent() << "}" << std::endl;
}

void champsim::json_printer::print(std::vector<phase_stats>& stats)
{
  stream << "[" << std::endl;
  ++indent_level;

  for (auto p : stats)
    print(p);

  stream << "]" << std::endl;
  --indent_level;
}
