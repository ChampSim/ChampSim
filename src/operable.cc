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
#include "champsim_constants.h"

champsim::operable::operable() : operable(champsim::chrono::picoseconds{1}) {}

champsim::operable::operable(champsim::chrono::picoseconds clock_period_) : clock_period(clock_period_) {}

void champsim::operable::operate_on(const champsim::chrono::clock& clock)
{
  while (next_operate < clock.now()) {
    _operate();
  }
}

void champsim::operable::_operate() {
  operate();
  ++current_cycle;
  next_operate += clock_period;
}
