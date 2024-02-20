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

namespace champsim
{
class operable
{
public:
  champsim::chrono::picoseconds clock_period{};
  champsim::chrono::clock::time_point current_time{};
  bool warmup = true;

  operable();
  virtual ~operable() = default;
  explicit operable(champsim::chrono::picoseconds clock_period);

  long _operate();
  long operate_on(const champsim::chrono::clock& clock);

  virtual void initialize() {} // LCOV_EXCL_LINE
  virtual long operate() = 0;
  virtual void begin_phase() {}                     // LCOV_EXCL_LINE
  virtual void end_phase(unsigned /*cpu index*/) {} // LCOV_EXCL_LINE
  virtual void print_deadlock() {}                  // LCOV_EXCL_LINE

  [[deprecated]] uint64_t current_cycle() const;
};

} // namespace champsim

#endif
