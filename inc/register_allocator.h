#include <array>
#include <cstdint>
#include <list>
#include <optional>
#include <queue>

#ifndef REG_ALLOC_H
#define REG_ALLOC_H

#include "instruction.h"

using PHYSICAL_REGISTER_ID = int16_t; //signed because we use -1 to indicate no physical register

struct physical_register {
  uint16_t arch_reg_index;
  uint64_t producing_instruction_id;
  bool valid; //has the producing instruction committed yet?
  bool busy;  //is this register in use anywhere in the pipeline?
};

class RegisterAllocator
{
private:
  std::array<PHYSICAL_REGISTER_ID, std::numeric_limits<uint8_t>::max() + 1> frontend_RAT, backend_RAT;
  std::queue<PHYSICAL_REGISTER_ID> free_registers;
  std::vector<physical_register> physical_register_file;

public:
  RegisterAllocator(uint16_t num_registers);
  PHYSICAL_REGISTER_ID rename_dest_register(int16_t reg, ooo_model_instr &instr);
  PHYSICAL_REGISTER_ID rename_src_register(int16_t reg);
  void complete_dest_register(PHYSICAL_REGISTER_ID physreg);
  void retire_dest_register(PHYSICAL_REGISTER_ID physreg);
  void free_register(PHYSICAL_REGISTER_ID physreg);
  bool isValid(PHYSICAL_REGISTER_ID physreg);
  unsigned long count_free_registers();
  void reset_frontend_RAT();
  void print_deadlock();
};
#endif
