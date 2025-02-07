/*
 *    Copyright 2026 The ChampSim Contributors
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

#include "instruction.h"

std::ostream& operator<<(std::ostream& os, const ooo_model_instr& instr)
{
  fmt::print(os, "{}: ", instr.ip);

  if (instr.destination_registers.empty() && instr.destination_memory.empty()) {
    fmt::print(os, "NOP    ");
  } else
    switch (instr.branch) {
    case NOT_BRANCH:
      fmt::print(os, "Instr   ");
      break;
    case BRANCH_DIRECT_JUMP:
      fmt::print(os, "Jump    ");
      break;
    case BRANCH_INDIRECT:
      fmt::print(os, "IndJump ");
      break;
    case BRANCH_CONDITIONAL:
      fmt::print(os, "CondBr  ");
      break;
    case BRANCH_DIRECT_CALL:
      fmt::print(os, "Call    ");
      break;
    case BRANCH_INDIRECT_CALL:
      fmt::print(os, "IndCall ");
      break;
    case BRANCH_RETURN:
      fmt::print(os, "Return  ");
      break;
    case BRANCH_OTHER:
      fmt::print(os, "OtherBr ");
      break;
    }

  bool needs_comma = false;
  if (instr.stack_pointer_folded) {
    if (needs_comma)
      fmt::print(os, ", ");
    fmt::print(os, "rsp (folded)");
    needs_comma = true;
  }
  for (auto x : instr.destination_registers) {
    if (needs_comma)
      fmt::print(os, ", ");
    if (instr.scheduled) {
      fmt::print(os, "p{}", x);
    } else {
      fmt::print(os, champsim::reg_names[x]);
      if (x == champsim::REG_INSTRUCTION_POINTER && instr.branch_mispredicted)
        fmt::print(os, " (mispredicted)");
    }
    needs_comma = true;
  }
  for (auto a : instr.destination_memory) {
    if (needs_comma)
      fmt::print(os, ", ");
    fmt::print(os, "[{}]", a);
    needs_comma = true;
  }

  fmt::print(os, " <- ");

  needs_comma = false;
  for (auto x : instr.source_registers) {
    if (needs_comma)
      fmt::print(os, ", ");
    if (instr.scheduled)
      fmt::print(os, "p{}", x);
    else
      fmt::print(os, champsim::reg_names[x]);
    needs_comma = true;
  }
  for (auto a : instr.source_memory) {
    if (needs_comma)
      fmt::print(os, ", ");
    fmt::print(os, "[{}]", a);
    needs_comma = true;
  }
  if (!needs_comma) {
    fmt::print(os, "Imm");
  }

  return os;
}
