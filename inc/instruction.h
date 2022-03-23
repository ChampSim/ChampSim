#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <vector>

#include "circular_buffer.hpp"
#include "util.h"

// instruction format
#define NUM_INSTR_DESTINATIONS_SPARC 4
#define NUM_INSTR_DESTINATIONS 2
#define NUM_INSTR_SOURCES 4

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

struct LSQ_ENTRY;

struct input_instr {
  // instruction pointer or PC (Program Counter)
  uint64_t ip = 0;

  // branch info
  uint8_t is_branch = 0;
  uint8_t branch_taken = 0;

  uint8_t destination_registers[NUM_INSTR_DESTINATIONS] = {}; // output registers
  uint8_t source_registers[NUM_INSTR_SOURCES] = {};           // input registers

  uint64_t destination_memory[NUM_INSTR_DESTINATIONS] = {}; // output memory
  uint64_t source_memory[NUM_INSTR_SOURCES] = {};           // input memory
};

struct cloudsuite_instr {
  // instruction pointer or PC (Program Counter)
  uint64_t ip = 0;

  // branch info
  uint8_t is_branch = 0;
  uint8_t branch_taken = 0;

  uint8_t destination_registers[NUM_INSTR_DESTINATIONS_SPARC] = {}; // output registers
  uint8_t source_registers[NUM_INSTR_SOURCES] = {};                 // input registers

  uint64_t destination_memory[NUM_INSTR_DESTINATIONS_SPARC] = {}; // output memory
  uint64_t source_memory[NUM_INSTR_SOURCES] = {};                 // input memory

  uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};
};

struct ooo_model_instr {
  uint64_t instr_id = 0;
  uint64_t ip = 0;
  uint64_t event_cycle = 0;

  bool is_branch = 0;
  bool branch_taken = 0;
  bool branch_prediction = 0;
  bool branch_mispredicted = 0; // A branch can be mispredicted even if the direction prediction is correct when the predicted target is not correct

  uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};

  uint8_t branch_type = NOT_BRANCH;
  uint64_t branch_target = 0;

  uint8_t fetched = 0;
  uint8_t decoded = 0;
  uint8_t scheduled = 0;
  uint8_t executed = 0;

  int num_mem_ops = 0;
  int num_reg_dependent = 0;

  std::vector<uint8_t> destination_registers = {}; // output registers
  std::vector<uint8_t> source_registers = {};      // input registers

  std::vector<uint64_t> destination_memory = {};
  std::vector<uint64_t> source_memory = {};

  // these are indices of instructions in the ROB that depend on me
  std::vector<std::reference_wrapper<ooo_model_instr>> registers_instrs_depend_on_me;

private:
  template <typename T>
  ooo_model_instr(T instr) : ip(instr.ip), is_branch(instr.is_branch), branch_taken(instr.branch_taken)
  {
    std::remove_copy(std::begin(instr.destination_registers), std::end(instr.destination_registers), std::back_inserter(this->destination_registers), 0);
    std::remove_copy(std::begin(instr.source_registers), std::end(instr.source_registers), std::back_inserter(this->source_registers), 0);
    std::remove_copy(std::begin(instr.destination_memory), std::end(instr.destination_memory), std::back_inserter(this->destination_memory), 0);
    std::remove_copy(std::begin(instr.source_memory), std::end(instr.source_memory), std::back_inserter(this->source_memory), 0);
  }

public:
  ooo_model_instr(uint8_t cpu, input_instr instr) : ooo_model_instr(instr)
  {
    asid[0] = cpu;
    asid[1] = cpu;
  }

  ooo_model_instr(uint8_t cpu, cloudsuite_instr instr) : ooo_model_instr(instr)
  {
    std::copy(std::begin(instr.asid), std::begin(instr.asid), std::begin(this->asid));
  }
};

#endif
