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

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "stats_printer.h"

namespace champsim
{
void to_json(nlohmann::json& j, const champsim::phase_stats& stats)
{
  std::vector<std::string> traces;
  std::transform(std::begin(stats.workloads), std::end(stats.workloads), std::back_inserter(traces), [](const auto& w) { return w.second; });
  j = nlohmann::json{{"name", stats.name}, {"traces", traces}, {"roi", stats.stats}};
}
} // namespace champsim

void champsim::json_printer::print(std::vector<phase_stats>& stats)
{
  // Indented rather than one dense line: these files are read by people at least as often as by
  // scripts, and a parser does not care either way.
  const nlohmann::json document = nlohmann::json::array_t{std::begin(stats), std::end(stats)};
  stream << document.dump(2) << '\n';
}
