#include "register_allocator.h"

#include <cassert>

RegisterAllocator::RegisterAllocator(uint32_t num_physical_registers)
{
  for (uint32_t i = 0; i < num_physical_registers; ++i) {
    free_registers.push(static_cast<PHYSICAL_REGISTER_ID>(i));
  }
}

PHYSICAL_REGISTER_ID RegisterAllocator::rename_dest_register(uint8_t reg, ooo_model_instr& instr)
{
  if (free_registers.size() == 0) {
    print_deadlock();
  }
  assert(free_registers.size() > 0);

  PHYSICAL_REGISTER_ID phys_reg = free_registers.front();
  free_registers.pop();

  // find the previous register allocated to the logical register
  auto previous = std::find_if(std::begin(used_registers), std::end(used_registers),
                               [reg](const physical_register& preg) { return preg.lreg_index == reg && preg.IsMostCurrentRename; });
  if (previous != std::end(used_registers)) {
    previous->IsMostCurrentRename = false;
  }

  frontend_RAT[reg] = phys_reg;
  used_registers.push_back({reg, phys_reg, instr.instr_id, 0, true});

  return phys_reg;
}

PHYSICAL_REGISTER_ID RegisterAllocator::rename_src_register(uint8_t reg)
{
  PHYSICAL_REGISTER_ID phys = frontend_RAT[reg];
  auto it = std::find_if(std::begin(used_registers), std::end(used_registers),
                         [phys, reg](const physical_register& preg) { return preg.lreg_index == reg && preg.preg_index == phys; });

  if (it == std::end(used_registers)) {
    // allocate the register if it hasn't yet been referenced
    PHYSICAL_REGISTER_ID phys_reg = free_registers.front();
    free_registers.pop();
    frontend_RAT[reg] = phys_reg;
    used_registers.push_back({reg, phys_reg, {}, 1, true});

    return phys_reg;
  } else {
    assert(it->IsMostCurrentRename);
    it->pending_consumers += 1;
  }

  return phys;
}

std::optional<uint64_t> RegisterAllocator::get_producing_instr(PHYSICAL_REGISTER_ID physreg)
{
  auto it = std::find_if(std::begin(used_registers), std::end(used_registers), [physreg](const physical_register& preg) { return preg.preg_index == physreg; });
  if (!(it->producing_instr_id.has_value()) || it == std::end(used_registers)) {
    return {};
  } else
    return it->producing_instr_id.value();
}

void RegisterAllocator::retire_dest_register(PHYSICAL_REGISTER_ID physreg)
{
  // the rename mapping is not guaranteed to be in the frontend RAT
  // find it in the physical register file
  auto it = std::find_if(std::begin(used_registers), std::end(used_registers), [physreg](const physical_register& preg) { return preg.preg_index == physreg; });

  assert(it != std::end(used_registers));
  uint8_t lreg = it->lreg_index;

  // the destination's physical register no longer has a producing instruction
  it->producing_instr_id.reset();

  // the instruction's retired, backend architectural state is updated
  backend_RAT[lreg] = physreg;
}

void RegisterAllocator::retire_src_register(PHYSICAL_REGISTER_ID physreg)
{
  // the rename mapping is not guaranteed to be in the frontend RAT
  // find it in the physical register file
  auto it = std::find_if(std::begin(used_registers), std::end(used_registers), [physreg](const physical_register& preg) { return preg.preg_index == physreg; });

  assert(it != std::end(used_registers));

  it->pending_consumers -= 1;

  assert(it->pending_consumers >= 0 || it->lreg_index == champsim::REG_STACK_POINTER);
}

void RegisterAllocator::free_retired_registers(std::deque<ooo_model_instr>& ROB)
{
  // find all used registers that have no pending consumers
  // and whose producing instruction has retired;
  // add their physical register to the free list
  auto it = std::begin(used_registers);
  while (it != std::end(used_registers)) {
    // must be superseded, have no more pending consumers,
    // either have no producing instruction listed, or the producing instruction must be retired
    if (!(it->IsMostCurrentRename) && (it->pending_consumers == 0)
        && (it->producing_instr_id.value_or(0) < std::begin(ROB)->instr_id)) {
      free_registers.push(it->preg_index);
      it = used_registers.erase(it);
    } else {
      it++;
    }
  }
}

unsigned long RegisterAllocator::count_free_registers() { return std::size(free_registers); }

void RegisterAllocator::reset_frontend_RAT()
{
  std::copy(std::begin(backend_RAT), std::end(backend_RAT), std::begin(frontend_RAT));
  // once wrong path is implemented
  // find registers allocated by WP instructions and free them
}

void RegisterAllocator::print_deadlock()
{
  for (auto i : used_registers) {
    if (i.producing_instr_id.has_value()) {
      fmt::print("Arch reg: {:3}\t Phys reg: {:3}\t pending_consumers: {:3}\t producing_instr_id: {}\t IsMostCurrentRename: {}\n",
                 static_cast<int>(i.lreg_index), static_cast<int>(i.preg_index), static_cast<int>(i.pending_consumers), i.producing_instr_id.value(),
                 i.IsMostCurrentRename);
    } else {
      fmt::print("Arch reg: {:3}\t Phys reg: {:3}\t pending_consumers: {:3}\t producing_instr_id: {}\t IsMostCurrentRename: {}\n",
                 static_cast<int>(i.lreg_index), static_cast<int>(i.preg_index), static_cast<int>(i.pending_consumers), false, i.IsMostCurrentRename);
    }
  }
  fmt::print("\n");
}
