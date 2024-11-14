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

#ifndef COMPONENT_H
#define COMPONENT_H

namespace champsim
{
class component
{
public:
  bool warmup = true;

  virtual void initialize() {} // LCOV_EXCL_LINE
  virtual void begin_phase() {}                     // LCOV_EXCL_LINE
  virtual void end_phase(unsigned /*cpu index*/) {} // LCOV_EXCL_LINE
  virtual void print_deadlock() {}                  // LCOV_EXCL_LINE
  virtual void print_dump() {}

};

} // namespace champsim

#endif