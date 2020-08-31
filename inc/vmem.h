#ifndef VMEM_H
#define VMEM_H

#include <iostream>
#include <deque>
#include <map>

#define VMEM_RAND_FACTOR 91827349653
// reserve 1MB of space
#define VMEM_RESERVE_CAPACITY 1048576

class VirtualMemory
{
 private:
  uint32_t num_cpus;
  uint32_t page_size;
  uint32_t log2_page_size;
  uint64_t num_ppages;
  std::deque<uint64_t> ppage_free_list;
  uint64_t get_next_free_ppage();

  std::map<uint64_t, uint64_t>* vpage_to_ppage_map;
  
  uint32_t pt_levels;
  std::map<uint64_t, uint64_t>** page_table;
  
  uint64_t rand_state;
  uint64_t vmem_rand();
 public:
  // capacity and pg_size are measured in bytes, and capacity must be a multiple of pg_size
  VirtualMemory(uint32_t number_of_cpus, uint64_t capacity, uint64_t pg_size, uint32_t page_table_levels, uint64_t random_seed);
  uint32_t get_paget_table_level_count();
  uint64_t va_to_pa(uint32_t cpu_num, uint64_t vaddr);
  uint64_t get_pte_pa(uint32_t cpu_num, uint64_t vaddr, uint32_t level);
};

#endif

