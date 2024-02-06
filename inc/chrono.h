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

#include <chrono>

namespace champsim
{
namespace chrono
{
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::chrono::nanoseconds;
using std::chrono::seconds;
using picoseconds = std::chrono::duration<std::intmax_t, std::pico>;

class clock
{
public:
  using duration = picoseconds;
  using time_point = std::chrono::time_point<clock>;
  using rep = typename duration::rep;
  using period = typename duration::period;

  constexpr static bool is_steady = false;
  time_point now() const noexcept;
  void tick(duration amount);

private:
  time_point m_now{};
};
} // namespace chrono
} // namespace champsim

#endif
