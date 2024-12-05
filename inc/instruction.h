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

#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <string_view>
#include <vector>

#include "address.h"
#include "champsim.h"
#include "chrono.h"
#include "trace_instruction.h"

// branch types
enum branch_type {
  BRANCH_DIRECT_JUMP = 0,
  BRANCH_INDIRECT,
  BRANCH_CONDITIONAL,
  BRANCH_DIRECT_CALL,
  BRANCH_INDIRECT_CALL,
  BRANCH_RETURN,
  BRANCH_OTHER,
  NOT_BRANCH
};

using PHYSICAL_REGISTER_ID = int16_t; // signed to use -1 to indicate no physical register

using namespace std::literals::string_view_literals;
inline constexpr std::array branch_type_names{"BRANCH_DIRECT_JUMP"sv, "BRANCH_INDIRECT"sv,      "BRANCH_CONDITIONAL"sv,
                                              "BRANCH_DIRECT_CALL"sv, "BRANCH_INDIRECT_CALL"sv, "BRANCH_RETURN"sv};

namespace champsim
{
template <typename T>
struct program_ordered {
  using id_type = uint64_t;
  id_type instr_id = 0;

  /**
   * Return a functor that matches this element's ID.
   * \overload
   */
  static auto matches_id(id_type id)
  {
    return [id](const T& instr) {
      return instr.instr_id == id;
    };
  }

  /**
   * Return a functor that matches this element's ID.
   */
  static auto matches_id(const T& instr) { return precedes(instr.instr_id); }

  /**
   * Order two elements of this type in the program.
   */
  static bool program_order(const T& lhs, const T& rhs) { return lhs.instr_id < rhs.instr_id; }

  /**
   * Return a functor that tests whether an instruction precededes the given instruction ID.
   * \overload
   */
  static auto precedes(id_type id)
  {
    return [id](const T& instr) {
      return instr.instr_id < id;
    };
  }

  /**
   * Return a functor that tests whether an instruction precededes the given instruction.
   */
  static auto precedes(const T& instr) { return precedes(instr.instr_id); }
};
} // namespace champsim

struct ooo_model_instr : champsim::program_ordered<ooo_model_instr> {
  champsim::address ip{};
  champsim::chrono::clock::time_point ready_time{};

  bool is_branch = false;
  bool branch_taken = false;
  bool branch_prediction = false;
  bool branch_mispredicted = false; // A branch can be mispredicted even if the direction prediction is correct when the predicted target is not correct

  std::array<uint8_t, 2> asid = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};

  branch_type branch{NOT_BRANCH};
  champsim::address branch_target{};

  bool dib_checked = false;
  bool fetch_issued = false;
  bool fetch_completed = false;
  bool decoded = false;
  bool scheduled = false;
  bool executed = false;
  bool completed = false;

  unsigned completed_mem_ops = 0;
  int num_reg_dependent = 0;

  std::vector<PHYSICAL_REGISTER_ID> destination_registers = {}; // output registers
  std::vector<PHYSICAL_REGISTER_ID> source_registers = {};      // input registers

  std::vector<champsim::address> destination_memory = {};
  std::vector<champsim::address> source_memory = {};

  // these are indices of instructions in the ROB that depend on me
  std::vector<std::reference_wrapper<ooo_model_instr>> registers_instrs_depend_on_me;

private:
  template <typename T>
  ooo_model_instr(T instr, std::array<uint8_t, 2> local_asid) : ip(instr.ip), is_branch(instr.is_branch), branch_taken(instr.branch_taken), asid(local_asid)
  {
    std::remove_copy(std::begin(instr.destination_registers), std::end(instr.destination_registers), std::back_inserter(this->destination_registers), 0);
    std::remove_copy(std::begin(instr.source_registers), std::end(instr.source_registers), std::back_inserter(this->source_registers), 0);

    auto dmem_end = std::remove(std::begin(instr.destination_memory), std::end(instr.destination_memory), uint64_t{0});
    std::transform(std::begin(instr.destination_memory), dmem_end, std::back_inserter(this->destination_memory), [](auto x) { return champsim::address{x}; });

    auto smem_end = std::remove(std::begin(instr.source_memory), std::end(instr.source_memory), uint64_t{0});
    std::transform(std::begin(instr.source_memory), smem_end, std::back_inserter(this->source_memory), [](auto x) { return champsim::address{x}; });

    bool writes_sp = std::count(std::begin(destination_registers), std::end(destination_registers), champsim::REG_STACK_POINTER);
    bool writes_ip = std::count(std::begin(destination_registers), std::end(destination_registers), champsim::REG_INSTRUCTION_POINTER);
    bool reads_sp = std::count(std::begin(source_registers), std::end(source_registers), champsim::REG_STACK_POINTER);
    bool reads_flags = std::count(std::begin(source_registers), std::end(source_registers), champsim::REG_FLAGS);
    bool reads_ip = std::count(std::begin(source_registers), std::end(source_registers), champsim::REG_INSTRUCTION_POINTER);
    bool reads_other = std::count_if(std::begin(source_registers), std::end(source_registers), [](uint8_t r) {
      return r != champsim::REG_STACK_POINTER && r != champsim::REG_FLAGS && r != champsim::REG_INSTRUCTION_POINTER;
    });

    // determine what kind of branch this is, if any
    if (!reads_sp && !reads_flags && writes_ip && !reads_other) {
      // direct jump
      is_branch = true;
      branch_taken = true;
      branch = BRANCH_DIRECT_JUMP;
    } else if (!reads_sp && !reads_ip && !reads_flags && writes_ip && reads_other) {
      // indirect branch
      is_branch = true;
      branch_taken = true;
      branch = BRANCH_INDIRECT;
    } else if (!reads_sp && reads_ip && !writes_sp && writes_ip && (reads_flags || reads_other)) {
      // conditional branch
      is_branch = true;
      branch_taken = instr.branch_taken; // don't change this
      branch = BRANCH_CONDITIONAL;
    } else if (reads_sp && reads_ip && writes_sp && writes_ip && !reads_flags && !reads_other) {
      // direct call
      is_branch = true;
      branch_taken = true;
      branch = BRANCH_DIRECT_CALL;
    } else if (reads_sp && reads_ip && writes_sp && writes_ip && !reads_flags && reads_other) {
      // indirect call
      is_branch = true;
      branch_taken = true;
      branch = BRANCH_INDIRECT_CALL;
    } else if (reads_sp && !reads_ip && writes_sp && writes_ip) {
      // return
      is_branch = true;
      branch_taken = true;
      branch = BRANCH_RETURN;
    } else if (writes_ip) {
      // some other branch type that doesn't fit the above categories
      is_branch = true;
      branch_taken = instr.branch_taken; // don't change this
      branch = BRANCH_OTHER;
    } else {
      branch_taken = false;
    }
  }

public:
  ooo_model_instr(uint8_t cpu, input_instr instr) : ooo_model_instr(instr, {cpu, cpu}) {}
  ooo_model_instr(uint8_t /*cpu*/, cloudsuite_instr instr) : ooo_model_instr(instr, {instr.asid[0], instr.asid[1]}) {}

  [[nodiscard]] std::size_t num_mem_ops() const { return std::size(destination_memory) + std::size(source_memory); }
};

#endif
