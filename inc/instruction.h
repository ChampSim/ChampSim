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
#include <iostream>
#include <limits>
#include <vector>

#include "champsim.h"
#include "address.h"
#include "trace_instruction.h"

// branch types
enum branch_type {
  NOT_BRANCH = 0,
  BRANCH_DIRECT_JUMP = 1,
  BRANCH_INDIRECT = 2,
  BRANCH_CONDITIONAL = 3,
  BRANCH_DIRECT_CALL = 4,
  BRANCH_INDIRECT_CALL = 5,
  BRANCH_RETURN = 6,
  BRANCH_OTHER = 7
};

struct ooo_model_instr {
  uint64_t instr_id = 0;
  champsim::address ip{};
  uint64_t event_cycle = 0;

  bool is_branch = 0;
  bool branch_taken = 0;
  bool branch_prediction = 0;
  bool branch_mispredicted = 0; // A branch can be mispredicted even if the direction prediction is correct when the predicted target is not correct

  uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};

  uint8_t branch_type = NOT_BRANCH;
  champsim::address branch_target{};

  uint8_t dib_checked = 0;
  uint8_t fetched = 0;
  uint8_t decoded = 0;
  uint8_t scheduled = 0;
  uint8_t executed = 0;

  unsigned completed_mem_ops = 0;
  int num_reg_dependent = 0;

  std::vector<uint8_t> destination_registers = {}; // output registers
  std::vector<uint8_t> source_registers = {};      // input registers

  std::vector<champsim::address> destination_memory = {};
  std::vector<champsim::address> source_memory = {};

  // these are indices of instructions in the ROB that depend on me
  std::vector<std::reference_wrapper<ooo_model_instr>> registers_instrs_depend_on_me;

private:
  class conversion_tag
  {
  };

  template <typename T>
  ooo_model_instr(conversion_tag, T instr) : ip(instr.ip), is_branch(instr.is_branch), branch_taken(instr.branch_taken)
  {
    std::remove_copy(std::begin(instr.destination_registers), std::end(instr.destination_registers), std::back_inserter(this->destination_registers), 0);
    std::remove_copy(std::begin(instr.source_registers), std::end(instr.source_registers), std::back_inserter(this->source_registers), 0);

    auto dmem_end = std::remove(std::begin(instr.destination_memory), std::end(instr.destination_memory), uint64_t{0});
    std::transform(std::begin(instr.destination_memory), dmem_end, std::back_inserter(this->destination_memory), [](auto x){ return champsim::address{x}; });

    auto smem_end = std::remove(std::begin(instr.source_memory), std::end(instr.source_memory), uint64_t{0});
    std::transform(std::begin(instr.source_memory), smem_end, std::back_inserter(this->source_memory), [](auto x){ return champsim::address{x}; });

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
      branch_type = BRANCH_DIRECT_JUMP;
    } else if (!reads_sp && !reads_flags && writes_ip && reads_other) {
      // indirect branch
      is_branch = true;
      branch_taken = true;
      branch_type = BRANCH_INDIRECT;
    } else if (!reads_sp && reads_ip && !writes_sp && writes_ip && reads_flags && !reads_other) {
      // conditional branch
      is_branch = true;
      branch_taken = instr.branch_taken; // don't change this
      branch_type = BRANCH_CONDITIONAL;
    } else if (reads_sp && reads_ip && writes_sp && writes_ip && !reads_flags && !reads_other) {
      // direct call
      is_branch = true;
      branch_taken = true;
      branch_type = BRANCH_DIRECT_CALL;
    } else if (reads_sp && reads_ip && writes_sp && writes_ip && !reads_flags && reads_other) {
      // indirect call
      is_branch = true;
      branch_taken = true;
      branch_type = BRANCH_INDIRECT_CALL;
    } else if (reads_sp && !reads_ip && writes_sp && writes_ip) {
      // return
      is_branch = true;
      branch_taken = true;
      branch_type = BRANCH_RETURN;
    } else if (writes_ip) {
      // some other branch type that doesn't fit the above categories
      is_branch = true;
      branch_taken = instr.branch_taken; // don't change this
      branch_type = BRANCH_OTHER;
    } else {
      branch_taken = false;
    }
  }

public:
  ooo_model_instr(uint8_t cpu, input_instr instr) : ooo_model_instr(conversion_tag{}, instr)
  {
    asid[0] = cpu;
    asid[1] = cpu;
  }

  ooo_model_instr(uint8_t, cloudsuite_instr instr) : ooo_model_instr(conversion_tag{}, instr)
  {
    std::copy(std::begin(instr.asid), std::begin(instr.asid), std::begin(this->asid));
  }

  std::size_t num_mem_ops() const { return std::size(destination_memory) + std::size(source_memory); }

  static bool program_order(const ooo_model_instr& lhs, const ooo_model_instr& rhs) { return lhs.instr_id < rhs.instr_id; }
};

#endif
