#ifndef TEST_INSTR_H
#define TEST_INSTR_H

#include "instruction.h"
#include "trace_instruction.h"

namespace champsim::test {
ooo_model_instr instruction_with_ip(uint64_t ip);
ooo_model_instr instruction_with_registers(uint8_t reg);

ooo_model_instr instruction_with_memory(std::array<uint64_t, NUM_INSTR_DESTINATIONS> dmem, std::array<uint64_t, NUM_INSTR_SOURCES> smem);

template <typename... Args>
auto instruction_with_source_memory(Args... args) {
  static_assert(sizeof...(args) <= NUM_INSTR_SOURCES);
  return instruction_with_memory({}, {args...});
}

template <typename... Args>
auto instruction_with_destination_memory(Args... args) {
  static_assert(sizeof...(args) <= NUM_INSTR_DESTINATIONS);
  return instruction_with_memory({args...}, {});
}
}

#endif
