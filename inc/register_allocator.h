#include <array>
#include <cstdint>
#include <list>
#include <optional>
#include <queue>

#ifndef REG_ALLOC_H
#define REG_ALLOC_H

#include "instruction.h"

using PHYSICAL_REGISTER_ID = uint8_t;

struct physical_register {
  uint8_t lreg_index;
  PHYSICAL_REGISTER_ID preg_index;
  std::optional<uint64_t> producing_instr_id;
  int32_t pending_consumers;
  bool IsMostCurrentRename;
};

class RegisterAllocator
{
private:
  std::array<PHYSICAL_REGISTER_ID, std::numeric_limits<uint8_t>::max() + 1> frontend_RAT, backend_RAT;
  std::queue<PHYSICAL_REGISTER_ID> free_registers;
  std::list<physical_register> used_registers;

public:
  RegisterAllocator(uint32_t num_registers);
  PHYSICAL_REGISTER_ID rename_dest_register(uint8_t reg, ooo_model_instr& instr);
  PHYSICAL_REGISTER_ID rename_src_register(uint8_t reg);
  std::optional<uint64_t> get_producing_instr(PHYSICAL_REGISTER_ID reg);
  void retire_dest_register(PHYSICAL_REGISTER_ID physreg);
  void retire_src_register(PHYSICAL_REGISTER_ID physreg);
  void free_retired_registers(std::deque<ooo_model_instr>& ROB);
  unsigned long count_free_registers();
  void reset_frontend_RAT();
  void print_deadlock();
};
#endif
