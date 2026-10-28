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

#ifndef MODULE_LIFECYCLE_H
#define MODULE_LIFECYCLE_H

#include <string>

namespace champsim
{

class stat_report;

// A module that participates in the run's phase lifecycle and reports its per-phase stats. Phase
// edges and stats are one concern -- stats accumulate within a phase and reset on its edge -- so a
// single mixin carries both. operable inherits it, so every operable is a lifecycle module; a
// non-operable reporter may inherit it directly. All hooks default to no-ops: a module overrides
// only what it needs, and the phase controller / stat collection see every lifecycle module uniformly.
struct module_lifecycle {
  virtual ~module_lifecycle() = default;

  // Phase edges. Ending a phase and reporting what it measured are one event -- the numbers are
  // final exactly then -- so end_phase finalises its counters and writes them into the report,
  // plaintext lines and JSON alike. A module that reports nothing publishes no statistics.
  virtual void begin_phase(bool /*warmup*/) {}
  virtual void end_phase(stat_report& /*out*/) {}

  // Stat identity (interface / model / instance name), stamped once at construction. It lets a phase
  // controller collect its governed modules' stats directly, without re-discovering them by interface.
  void set_stat_identity(std::string interface_name, std::string model, std::string name)
  {
    stat_interface_ = std::move(interface_name);
    stat_model_ = std::move(model);
    stat_name_ = std::move(name);
  }
  [[nodiscard]] const std::string& stat_interface() const { return stat_interface_; }
  [[nodiscard]] const std::string& stat_model() const { return stat_model_; }
  [[nodiscard]] const std::string& stat_name() const { return stat_name_; }

private:
  std::string stat_interface_;
  std::string stat_model_;
  std::string stat_name_;
};

} // namespace champsim

#endif // MODULE_LIFECYCLE_H
