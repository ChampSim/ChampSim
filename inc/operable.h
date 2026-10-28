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

#ifndef OPERABLE_H
#define OPERABLE_H

#include "chrono.h"
#include "module_lifecycle.h"

namespace champsim
{
// Every operable participates in the phase lifecycle (begin/end phase, per-phase stats), so it
// auto-enrolls in module_lifecycle; a module overrides only the hooks it needs.
class operable : public module_lifecycle
{
public:
  champsim::chrono::picoseconds clock_period{};
  champsim::chrono::clock::time_point current_time{};

  operable();
  virtual ~operable() = default;
  explicit operable(champsim::chrono::picoseconds clock_period);

  long _operate();
  long operate_on(const champsim::chrono::clock& clock);

  virtual void initialize() {} // LCOV_EXCL_LINE
  virtual long operate() = 0;

  // Idle-skip hook: return 0 to simulate this cycle, n>0 to skip n cycles (no progress). Must still tick per-cycle submodule hooks on skips, and
  // skip at most 1 cycle if work can arrive by external push. Disabled globally via set_skip_enabled(false) / config "cycle_skip".
  virtual long poll_cycle() { return 0; } // LCOV_EXCL_LINE

  virtual void print_deadlock() {} // LCOV_EXCL_LINE
  virtual void end_simulation() {} // LCOV_EXCL_LINE

  static void set_skip_enabled(bool enabled);
  static bool skip_enabled();

  [[deprecated]] uint64_t current_cycle() const;
};

} // namespace champsim

#endif
