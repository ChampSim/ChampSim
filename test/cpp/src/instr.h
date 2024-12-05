#ifndef TEST_INSTR_H
#define TEST_INSTR_H

#include "instruction.h"

namespace champsim::test {
ooo_model_instr instruction_with_ip(champsim::address ip);
ooo_model_instr instruction_with_ip(uint64_t ip);
ooo_model_instr branch_instruction_with_ip(champsim::address ip);
ooo_model_instr branch_instruction_with_ip(uint64_t ip);
ooo_model_instr instruction_with_registers(uint8_t reg);
ooo_model_instr instruction_with_ip_and_source_memory(champsim::address ip, champsim::address smem);
}

#endif
