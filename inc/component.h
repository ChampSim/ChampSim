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

#include <string_view>
#include <vector>
#include <fmt/ranges.h>

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

  constexpr static std::string_view param_fmtstr{"{} {} {:35} {:>1}\n"};
  constexpr static std::string_view params_fmtstr{"{} {} {:35} [{:>1}]\n"};

  template<typename T>
  void print_parameter(std::string type_name, std::string instance_name, std::string parameter_name, T parameter) {
    fmt::print(param_fmtstr,type_name,instance_name,parameter_name,parameter);
  }

  template<typename T>
  void print_parameters(std::string type_name, std::string instance_name, std::string parameter_name, std::vector<T> parameters) {
    fmt::print(params_fmtstr,type_name,instance_name,parameter_name,fmt::join(parameters,", "));
  }
};

} // namespace champsim

#endif