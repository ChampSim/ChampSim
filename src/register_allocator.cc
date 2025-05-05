#include "register_allocator.h"

#include <cassert>

RegisterAllocator::RegisterAllocator(size_t num_physical_registers)
{
  assert(num_physical_registers <= std::numeric_limits<PHYSICAL_REGISTER_ID>::max());
  for (size_t i = 0; i < num_physical_registers; ++i) {
    free_registers.push(static_cast<PHYSICAL_REGISTER_ID>(i));
  }
  physical_register_file = std::vector<physical_register>(num_physical_registers, {0, 0, 0, false, false});
  frontend_RAT.fill(-1); // default value for no mapping
  backend_RAT.fill(-1);
}

PHYSICAL_REGISTER_ID RegisterAllocator::rename_dest_register(int16_t reg, champsim::program_ordered<ooo_model_instr>::id_type producer_id,
                                                             uint64_t producer_kanata_id)
{
  assert(!free_registers.empty());

  PHYSICAL_REGISTER_ID physreg_id = free_registers.front();
  free_registers.pop();
  frontend_RAT[reg] = physreg_id;
  physical_register physreg{};
  physreg.arch_reg_index = (uint16_t)reg;
  physreg.producing_instruction_id = producer_id;
  physreg.producing_instruction_kanata_id = producer_kanata_id;
  physreg.valid = false;
  physreg.busy = true;
  physical_register_file.at(physreg_id) = physreg;

  return physreg_id;
}

SrcRegisterRenameResult RegisterAllocator::rename_src_register(int16_t reg)
{
  PHYSICAL_REGISTER_ID physreg_id = frontend_RAT[reg];

  if (physreg_id < 0) {
    // allocate the register if it hasn't yet been mapped
    // (common due to the traces being slices in the middle of a program)
    physreg_id = free_registers.front();
    free_registers.pop();
    frontend_RAT[reg] = physreg_id;
    backend_RAT[reg] = physreg_id; // we assume this register's last write has been committed
    physical_register physreg{};
    physreg.arch_reg_index = (uint16_t)reg;
    physreg.producing_instruction_id = UINT64_MAX;
    physreg.producing_instruction_kanata_id = UINT64_MAX;
    physreg.valid = true;
    physreg.busy = true;
    physical_register_file.at(physreg_id) = physreg;
  }

  return {physreg_id, physical_register_file.at(physreg_id).producing_instruction_kanata_id};
}

void RegisterAllocator::complete_dest_register(PHYSICAL_REGISTER_ID physreg)
{
  // mark the physical register as valid
  physical_register_file.at(physreg).valid = true;
}

void RegisterAllocator::retire_dest_register(PHYSICAL_REGISTER_ID physreg)
{
  // grab the arch reg index, find old phys reg in backend RAT
  uint16_t arch_reg = physical_register_file.at(physreg).arch_reg_index;
  PHYSICAL_REGISTER_ID old_physreg_id = backend_RAT[arch_reg];

  // update the backend RAT with the new phys reg
  backend_RAT[arch_reg] = physreg;

  // free the old phys reg
  if (old_physreg_id != -1) {
    free_register(old_physreg_id);
  }
}

void RegisterAllocator::free_register(PHYSICAL_REGISTER_ID physreg_id)
{
  physical_register physreg{};
  physreg.arch_reg_index = 255;
  physreg.producing_instruction_id = 0;
  physreg.producing_instruction_kanata_id = 0;
  physreg.valid = false;
  physreg.busy = false;
  physical_register_file.at(physreg_id) = physreg;
  free_registers.push(physreg_id);
}

bool RegisterAllocator::isValid(PHYSICAL_REGISTER_ID physreg_id) const { return physical_register_file.at(physreg_id).valid; }

bool RegisterAllocator::isAllocated(PHYSICAL_REGISTER_ID archreg) const { return frontend_RAT[archreg] != -1; }

unsigned long RegisterAllocator::count_free_registers() const { return std::size(free_registers); }

int RegisterAllocator::count_reg_dependencies(const ooo_model_instr& instr) const
{
  return static_cast<int>(std::count_if(std::begin(instr.source_registers), std::end(instr.source_registers), [this](auto reg) { return !isValid(reg); }));
}

void RegisterAllocator::reset_frontend_RAT()
{
  std::copy(std::begin(backend_RAT), std::end(backend_RAT), std::begin(frontend_RAT));
  // once wrong path is implemented:
  // find registers allocated by wrong-path instructions and free them
}

void RegisterAllocator::print_deadlock()
{
  fmt::print("Frontend Register Allocation Table        Backend Register Allocation Table\n");
  for (size_t i = 0; i < frontend_RAT.size(); ++i) {
    fmt::print("Arch reg: {:3}    Phys reg: {:3}            Arch reg: {:3}    Phys reg: {:3}\n", i, frontend_RAT[i], i, backend_RAT[i]);
  }

  if (count_free_registers() == 0) {
    fmt::print("\n**WARNING!! WARNING!!** THE PHYSICAL REGISTER FILE IS COMPLETELY OCCUPIED.\n");
    fmt::print("It is extremely likely your register file size is too small.\n");
  }

  fmt::print("\nPhysical Register File\n");
  for (size_t i = 0; i < physical_register_file.size(); ++i) {
    fmt::print("Phys reg: {:3}\t Arch reg: {:3}\t Producer: {}\t Valid: {}\t Busy: {}\n", static_cast<int>(i),
               static_cast<int>(physical_register_file.at(i).arch_reg_index), physical_register_file.at(i).producing_instruction_id,
               physical_register_file.at(i).valid, physical_register_file.at(i).busy);
  }
  fmt::print("\n");
}
