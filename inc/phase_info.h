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
#include <string>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>

namespace champsim
{

// A phase of the run: a name, a warmup flag, an ROI flag, and a length
// denominated in each consumer's own progress unit (instructions for
// cores, packets for a network consumer, ...). roi selects whether the phase
// contributes to region-of-interest statistics — typically !is_warmup, but
// independent so a run can contain unmeasured non-warmup phases (e.g. a
// fast-forward between warmup and the measured region). Workload identity
// (e.g. trace paths) is not part of the phase — producers describe themselves
// via packet_producer::describe().
struct phase_info {
  std::string name;
  bool is_warmup;
  bool roi;
  uint64_t length;
};

// One measured phase's statistics: what its governed modules reported, in both output formats.
// A phase is the measurement window, so there is exactly one set of numbers per entry.
struct phase_stats {
  std::string name;
  // (consumer id, workload description) -- the id comes from the consumer being fed, not from
  // counting producers, so a consumer with two producers names both against itself.
  std::vector<std::pair<int, std::string>> workloads;
  std::vector<std::string> lines; // every reporting module's plaintext lines, in interface order
  nlohmann::json stats;           // [interface][model][instance name]
};

} // namespace champsim

#endif
