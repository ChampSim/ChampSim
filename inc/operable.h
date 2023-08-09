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

namespace champsim
{

class operable
{
public:
  const double CLOCK_SCALE;

  double leap_operation = 0;
  uint64_t current_cycle = 0;
  bool warmup = true;

  explicit operable(double scale) : CLOCK_SCALE(scale - 1) {}

  long _operate()
  {
    // skip periodically
    if (leap_operation >= 1) {
      leap_operation -= 1;
      return 0;
    }

    auto result = operate();

    leap_operation += CLOCK_SCALE;
    ++current_cycle;

    return result;
  }

  virtual void initialize() {} // LCOV_EXCL_LINE
  virtual long operate() = 0;
  virtual void begin_phase() {}       // LCOV_EXCL_LINE
  virtual void end_phase(unsigned) {} // LCOV_EXCL_LINE
  virtual void print_deadlock() {}    // LCOV_EXCL_LINE
};

} // namespace champsim

#endif
