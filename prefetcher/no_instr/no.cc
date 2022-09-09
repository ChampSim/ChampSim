#include "cache.h"

void CACHE::prefetcher_initialize() {}

void CACHE::prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target) {}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)
{
  assert(addr == ip); // Invariant for instruction prefetchers
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_final_stats() {}
