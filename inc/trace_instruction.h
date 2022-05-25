#ifndef TRACE_INSTRUCTION_H
#define TRACE_INSTRUCTION_H

#include <limits>

// instruction format
constexpr std::size_t NUM_INSTR_DESTINATIONS_SPARC = 4;
constexpr std::size_t NUM_INSTR_DESTINATIONS = 2;
constexpr std::size_t NUM_INSTR_SOURCES = 4;

class LSQ_ENTRY;

struct input_instr {
  // instruction pointer or PC (Program Counter)
  unsigned long long ip = 0;

  // branch info
  unsigned char is_branch = 0;
  unsigned char branch_taken = 0;

  unsigned char destination_registers[NUM_INSTR_DESTINATIONS] = {}; // output registers
  unsigned char source_registers[NUM_INSTR_SOURCES] = {};           // input registers

  unsigned long long destination_memory[NUM_INSTR_DESTINATIONS] = {}; // output memory
  unsigned long long source_memory[NUM_INSTR_SOURCES] = {};           // input memory
};

struct cloudsuite_instr {
  // instruction pointer or PC (Program Counter)
  unsigned long long ip = 0;

  // branch info
  unsigned char is_branch = 0;
  unsigned char branch_taken = 0;

  unsigned char destination_registers[NUM_INSTR_DESTINATIONS_SPARC] = {}; // output registers
  unsigned char source_registers[NUM_INSTR_SOURCES] = {};                 // input registers

  unsigned long long destination_memory[NUM_INSTR_DESTINATIONS_SPARC] = {}; // output memory
  unsigned long long source_memory[NUM_INSTR_SOURCES] = {};                 // input memory

  unsigned char asid[2] = {std::numeric_limits<unsigned char>::max(), std::numeric_limits<unsigned char>::max()};
};

#endif

