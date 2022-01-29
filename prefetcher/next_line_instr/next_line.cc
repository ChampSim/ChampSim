#include <iostream>

#include "ooo_cpu.h"

void O3_CPU::prefetcher_initialize() { std::cout << "CPU " << cpu << " next line instruction prefetcher" << endl; }

void O3_CPU::prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target) {}

uint32_t O3_CPU::prefetcher_cache_operate(uint64_t v_addr, uint8_t cache_hit, uint8_t prefetch_hit, uint32_t metadata_in)
{
  uint64_t pf_addr = v_addr + (1 << LOG2_BLOCK_SIZE);
  prefetch_code_line(pf_addr);
  return metadata_in;
}

void O3_CPU::prefetcher_cycle_operate() {}

uint32_t O3_CPU::prefetcher_cache_fill(uint64_t v_addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_v_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void O3_CPU::l1i_prefetcher_final_stats() {}
