/*
 *    Copyright 2026 The ChampSim Contributors
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

#ifndef STAT_REPORT_H
#define STAT_REPORT_H

#include <string>
#include <utility>
#include <vector>
#include <nlohmann/json.hpp>

#include "json_stat_builder.h"

namespace champsim
{

// One module's statistics for one phase: the plaintext lines it contributes and the JSON object it
// fills. A module writes both through a single hook, so a stat report is one call rather than one
// call per output format.
class stat_report
{
public:
  // Append one plaintext line.
  void line(std::string text) { lines_.push_back(std::move(text)); }

  // A builder writing into this report's JSON object. Returned by value: it borrows the object, so
  // copies and nested group() builders all write to the same place.
  json_stat_builder json() { return json_stat_builder{json_}; }

  [[nodiscard]] const std::vector<std::string>& text() const { return lines_; }
  [[nodiscard]] const nlohmann::json& json_object() const { return json_; }

  // True when the module reported nothing at all -- it publishes no statistics.
  [[nodiscard]] bool empty() const { return lines_.empty() && json_.empty(); }

private:
  std::vector<std::string> lines_;
  nlohmann::json json_ = nlohmann::json::object();
};

} // namespace champsim

#endif // STAT_REPORT_H
