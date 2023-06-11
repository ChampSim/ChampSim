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

#ifndef CHRONO_H
#define CHRONO_H

#include <cfenv>
#include <chrono>
#include <cmath>

namespace champsim
{
namespace chrono
{
using picoseconds = std::chrono::duration<std::intmax_t, std::pico>;
uint64_t cycles(picoseconds ns);

template <typename T, typename U>
uint64_t cycles(T ns, U period)
{
  std::fesetround(FE_UPWARD);
  auto result = std::lrint(ns / period);
  return result < 0 ? 0 : static_cast<uint64_t>(result);
}
} // namespace chrono

struct global_clock_period {
  static chrono::picoseconds value;
};
} // namespace champsim

#endif
