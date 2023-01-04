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

#include <iostream>

namespace champsim
{

class operable
{
public:
  const double CLOCK_SCALE;

  double leap_operation = 0;
  uint64_t current_cycle = 0;

  explicit operable(double scale) : CLOCK_SCALE(scale - 1) {}

  void _operate()
  {
    // skip periodically
    if (leap_operation >= 1) {
      leap_operation -= 1;
      return;
    }

    operate();

    leap_operation += CLOCK_SCALE;
    ++current_cycle;
  }

  virtual void operate() = 0;
  virtual void print_deadlock() {}
};

class by_next_operate
{
public:
  bool operator()(operable* lhs, operable* rhs) const { return lhs->leap_operation < rhs->leap_operation; }
};

} // namespace champsim

#endif
