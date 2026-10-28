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

#include "operable.h"

champsim::operable::operable() : operable(champsim::chrono::picoseconds{1}) {}

champsim::operable::operable(champsim::chrono::picoseconds clock_period_) : clock_period(clock_period_) {}

namespace
{
// Process-wide idle-skip switch (single-threaded simulator). Default on;
// config root key "cycle_skip": false forces every poll_cycle() to be ignored
// so behavior can be A/B verified against the always-operate baseline.
bool skip_enabled_ = true;
} // namespace

void champsim::operable::set_skip_enabled(bool enabled) { skip_enabled_ = enabled; }
bool champsim::operable::skip_enabled() { return skip_enabled_; }

long champsim::operable::operate_on(const champsim::chrono::clock& clock)
{
  long progress{0};
  while (current_time < clock.now()) {
    // Enter the next local cycle before polling so poll_cycle() observes the
    // same end-of-cycle timestamp that operate() does (matching _operate()).
    current_time += clock_period;
    if (skip_enabled_) {
      if (long skip = poll_cycle(); skip > 0) {
        current_time += (skip - 1) * clock_period; // this cycle plus (skip - 1) more
        continue;
      }
    }
    progress += operate();
  }

  return progress;
}

long champsim::operable::_operate()
{
  current_time += clock_period;
  return operate();
}

uint64_t champsim::operable::current_cycle() const { return static_cast<uint64_t>(current_time.time_since_epoch() / clock_period); }
