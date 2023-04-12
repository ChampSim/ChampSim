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

#ifndef TRACEREADER_H
#define TRACEREADER_H

#include <memory>
#include <numeric>
#include <string>

#include "instruction.h"

namespace champsim
{
class tracereader
{
  static uint64_t instr_unique_id;
  struct reader_concept {
    virtual ~reader_concept() = default;
    virtual ooo_model_instr operator()() = 0;
  };

  template <typename T>
  struct reader_model final : public reader_concept {
    T intern_;
    reader_model(T&& val) : intern_(std::move(val)) {}

    ooo_model_instr operator()() override { return intern_(); }
  };

  std::unique_ptr<reader_concept> pimpl_;

public:
  template <typename T>
  tracereader(T&& val) : pimpl_(std::make_unique<reader_model<T>>(std::move(val)))
  {
  }

  auto operator()()
  {
    auto retval = (*pimpl_)();
    retval.instr_id = instr_unique_id++;
    return retval;
  }
};

std::string get_fptr_cmd(std::string_view fname);
ooo_model_instr apply_branch_target(ooo_model_instr branch, const ooo_model_instr& target);

template <typename It>
void set_branch_targets(It begin, It end)
{
  std::reverse_iterator rbegin{end}, rend{begin};
  std::adjacent_difference(rbegin, rend, rbegin, apply_branch_target);
}
} // namespace champsim

champsim::tracereader get_tracereader(std::string fname, uint8_t cpu, bool is_cloudsuite);

#endif
