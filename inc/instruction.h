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

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include "circular_buffer.hpp"
#include "trace_instruction.h"

// special registers that help us identify branches
#define REG_STACK_POINTER 6
#define REG_FLAGS 25
#define REG_INSTRUCTION_POINTER 26

// branch types
#define NOT_BRANCH 0
#define BRANCH_DIRECT_JUMP 1
#define BRANCH_INDIRECT 2
#define BRANCH_CONDITIONAL 3
#define BRANCH_DIRECT_CALL 4
#define BRANCH_INDIRECT_CALL 5
#define BRANCH_RETURN 6
#define BRANCH_OTHER 7

struct ooo_model_instr {
  uint64_t instr_id = 0, ip = 0, event_cycle = 0;

  bool is_branch = 0, is_memory = 0, branch_taken = 0, branch_mispredicted = 0, source_added[NUM_INSTR_SOURCES] = {},
       destination_added[NUM_INSTR_DESTINATIONS_SPARC] = {};

  uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};

  uint8_t branch_type = NOT_BRANCH;
  uint64_t branch_target = 0;

  uint8_t translated = 0, fetched = 0, decoded = 0, scheduled = 0, executed = 0;
  int num_reg_ops = 0, num_mem_ops = 0, num_reg_dependent = 0;

  uint8_t destination_registers[NUM_INSTR_DESTINATIONS_SPARC] = {}; // output registers

  uint8_t source_registers[NUM_INSTR_SOURCES] = {}; // input registers

  // these are indices of instructions in the ROB that depend on me
  std::vector<champsim::circular_buffer<ooo_model_instr>::iterator> registers_instrs_depend_on_me, memory_instrs_depend_on_me;

  // memory addresses that may cause dependencies between instructions
  uint64_t instruction_pa = 0;
  uint64_t destination_memory[NUM_INSTR_DESTINATIONS_SPARC] = {}; // output memory
  uint64_t source_memory[NUM_INSTR_SOURCES] = {};                 // input memory

  std::array<std::vector<LSQ_ENTRY>::iterator, NUM_INSTR_SOURCES> lq_index = {};
  std::array<std::vector<LSQ_ENTRY>::iterator, NUM_INSTR_DESTINATIONS_SPARC> sq_index = {};

  ooo_model_instr() = default;

  ooo_model_instr(uint8_t cpu, input_instr instr)
  {
    std::copy(std::begin(instr.destination_registers), std::end(instr.destination_registers), std::begin(this->destination_registers));
    std::copy(std::begin(instr.destination_memory), std::end(instr.destination_memory), std::begin(this->destination_memory));
    std::copy(std::begin(instr.source_registers), std::end(instr.source_registers), std::begin(this->source_registers));
    std::copy(std::begin(instr.source_memory), std::end(instr.source_memory), std::begin(this->source_memory));

    this->ip = instr.ip;
    this->is_branch = instr.is_branch;
    this->branch_taken = instr.branch_taken;

    asid[0] = cpu;
    asid[1] = cpu;
  }

  ooo_model_instr(uint8_t cpu, cloudsuite_instr instr)
  {
    std::copy(std::begin(instr.destination_registers), std::end(instr.destination_registers), std::begin(this->destination_registers));
    std::copy(std::begin(instr.destination_memory), std::end(instr.destination_memory), std::begin(this->destination_memory));
    std::copy(std::begin(instr.source_registers), std::end(instr.source_registers), std::begin(this->source_registers));
    std::copy(std::begin(instr.source_memory), std::end(instr.source_memory), std::begin(this->source_memory));

    this->ip = instr.ip;
    this->is_branch = instr.is_branch;
    this->branch_taken = instr.branch_taken;

    std::copy(std::begin(instr.asid), std::begin(instr.asid), std::begin(this->asid));
  }
};

#endif
