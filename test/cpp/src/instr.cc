#include "instr.h"

ooo_model_instr champsim::test::instruction_with_ip(uint64_t ip)
{
  input_instr i;
  i.ip = ip;
  i.is_branch = false;
  i.branch_taken = false;

  std::fill(std::begin(i.destination_registers), std::end(i.destination_registers), 0);
  std::fill(std::begin(i.source_registers), std::end(i.source_registers), 0);

  std::fill(std::begin(i.destination_memory), std::end(i.destination_memory), 0);
  std::fill(std::begin(i.source_memory), std::end(i.source_memory), 0);
  return ooo_model_instr{0, i};
}

ooo_model_instr champsim::test::instruction_with_registers(uint8_t reg)
{
  input_instr i;
  i.ip = 1;
  i.is_branch = false;
  i.branch_taken = false;

  std::fill(std::begin(i.destination_registers), std::end(i.destination_registers), 0);
  std::fill(std::begin(i.source_registers), std::end(i.source_registers), 0);

  i.destination_registers[0] = reg;
  i.source_registers[0] = reg;

  std::fill(std::begin(i.destination_memory), std::end(i.destination_memory), 0);
  std::fill(std::begin(i.source_memory), std::end(i.source_memory), 0);
  return ooo_model_instr{0, i};
}
