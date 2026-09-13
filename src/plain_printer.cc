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

#include <iterator>
#include <string>
#include <vector>
#include <fmt/core.h>
#include <fmt/ostream.h>

#include "stats_printer.h"

void champsim::plain_printer::print(champsim::phase_stats& stats)
{
  auto lines = format(stats);
  std::copy(std::begin(lines), std::end(lines), std::ostream_iterator<std::string>(stream, "\n"));
}

std::vector<std::string> champsim::plain_printer::format(champsim::phase_stats& stats)
{
  std::vector<std::string> lines{};
  lines.push_back(fmt::format("=== {} ===", stats.name));

  for (const auto& [consumer, description] : stats.workloads) {
    lines.push_back(fmt::format("CPU {} runs {}", consumer, description));
  }

  lines.emplace_back("");
  lines.emplace_back("Region of Interest Statistics");
  lines.emplace_back("");
  lines.insert(lines.end(), stats.lines.begin(), stats.lines.end());

  return lines;
}

void champsim::plain_printer::print(std::vector<phase_stats>& stats)
{
  for (auto p : stats) {
    print(p);
  }
}
