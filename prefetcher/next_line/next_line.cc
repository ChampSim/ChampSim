#include "next_line.h"
#include <iostream>

void next_line::prefetcher_initialize() { std::cout << intern_->NAME << " next line prefetcher" << std::endl; }
uint32_t next_line::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)
{
  // assert(addr == ip); // Invariant for instruction prefetchers
  uint64_t pf_addr = addr + (1 << LOG2_BLOCK_SIZE);
  prefetch_line(pf_addr, true, metadata_in);
  return metadata_in;
}

uint32_t next_line::prefetcher_cache_fill(uint64_t addr, long set, long way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}
