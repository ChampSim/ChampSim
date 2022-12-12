#ifndef VMEM_H
#define VMEM_H

#include <cstdint>
#include <deque>
#include <map>

#include "champsim.h"
#include "address.h"

class MEMORY_CONTROLLER;

// reserve 1MB of space
inline constexpr uint64_t VMEM_RESERVE_CAPACITY = 1048576;

inline constexpr std::size_t PTE_BYTES = 8;

class VirtualMemory
{
private:
  std::map<std::pair<uint32_t, champsim::page_number>, champsim::address> vpage_to_ppage_map;
  std::map<std::tuple<uint32_t, uint32_t, champsim::address_slice<champsim::dynamic_extent>>, champsim::address> page_table;

  champsim::address next_pte_page{};

  champsim::address next_ppage;
  champsim::address last_ppage;

  champsim::address ppage_front() const;
  void ppage_pop();

public:
  const uint64_t minor_fault_penalty;
  const std::size_t pt_levels;
  const uint64_t pte_page_size; // Size of a PTE page

  // capacity and pg_size are measured in bytes, and capacity must be a multiple of pg_size
  VirtualMemory(unsigned paddr_bits, uint64_t pg_size, std::size_t page_table_levels, uint64_t minor_penalty, MEMORY_CONTROLLER& dram);
  uint64_t shamt(std::size_t level) const;
  uint64_t get_offset(champsim::address vaddr, std::size_t level) const;
  std::size_t available_ppages() const;
  std::pair<champsim::address, uint64_t> va_to_pa(uint32_t cpu_num, champsim::address vaddr);
  std::pair<champsim::address, uint64_t> get_pte_pa(uint32_t cpu_num, champsim::address vaddr, std::size_t level);
};

#endif
