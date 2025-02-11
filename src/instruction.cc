#include "instruction.h"

std::ostream& operator<<(std::ostream& os, const ooo_model_instr& instr)
{
  fmt::print(os, "{}: ", instr.ip);

  if (instr.destination_registers.empty() && instr.destination_memory.empty()) {
    os << "NOP    ";
  } else
    switch (instr.branch) {
    case NOT_BRANCH:
      os << "Instr   ";
      break;
    case BRANCH_DIRECT_JUMP:
      os << "Jump    ";
      break;
    case BRANCH_INDIRECT:
      os << "IndJump ";
      break;
    case BRANCH_CONDITIONAL:
      os << "CondBr  ";
      break;
    case BRANCH_DIRECT_CALL:
      os << "Call    ";
      break;
    case BRANCH_INDIRECT_CALL:
      os << "IndCall ";
      break;
    case BRANCH_RETURN:
      os << "Return  ";
      break;
    case BRANCH_OTHER:
      os << "OtherBr ";
      break;
    }

  bool needs_comma = false;
  if (instr.stack_pointer_folded) {
    if (needs_comma)
      os << ", ";
    os << "rsp (folded)";
    needs_comma = true;
  }
  for (auto x : instr.destination_registers) {
    if (needs_comma)
      os << ", ";
    if (instr.scheduled) {
      fmt::print(os, "p{}", x);
    } else {
      os << champsim::reg_names[x];
      if (x == champsim::REG_INSTRUCTION_POINTER && instr.branch_mispredicted)
        os << " (mispredicted)";
    }
    needs_comma = true;
  }
  for (auto a : instr.destination_memory) {
    if (needs_comma)
      os << ", ";
    fmt::print(os, "[{}]", a);
    needs_comma = true;
  }

  os << " <- ";

  needs_comma = false;
  for (auto x : instr.source_registers) {
    if (needs_comma)
      os << ", ";
    if (instr.scheduled)
      fmt::print(os, "p{}", x);
    else
      os << champsim::reg_names[x];
    needs_comma = true;
  }
  for (auto a : instr.source_memory) {
    if (needs_comma)
      os << ", ";
    fmt::print(os, "[{}]", a);
    needs_comma = true;
  }
  if (!needs_comma) {
    os << "Imm";
  }

  return os;
}
