#ifndef VMEM_H
#define VMEM_H

#include <cstdint>
#include <deque>
#include <map>

// reserve 1MB of space
#define VMEM_RESERVE_CAPACITY 1048576

#define PTE_BYTES 8

class VirtualMemory
{
private:
  std::map<std::pair<uint32_t, uint64_t>, uint64_t> vpage_to_ppage_map;
  std::map<std::tuple<uint32_t, uint64_t, uint32_t>, uint64_t> page_table;

  uint64_t next_pte_page = 0;

  std::deque<uint64_t> ppage_free_list;

public:
  const uint64_t minor_fault_penalty;
  const uint32_t pt_levels;
  const uint32_t page_size; // Size of a PTE page

  // capacity and pg_size are measured in bytes, and capacity must be a multiple
  // of pg_size
  VirtualMemory(unsigned paddr_bits, uint64_t pg_size, uint32_t page_table_levels, uint64_t random_seed, uint64_t minor_fault_penalty);
  uint64_t shamt(uint32_t level) const;
  uint64_t get_offset(uint64_t vaddr, uint32_t level) const;
  std::size_t available_ppages() const { return std::size(ppage_free_list); }
  std::pair<uint64_t, uint64_t> va_to_pa(uint32_t cpu_num, uint64_t vaddr);
  std::pair<uint64_t, uint64_t> get_pte_pa(uint32_t cpu_num, uint64_t vaddr, uint32_t level);
};

#endif
