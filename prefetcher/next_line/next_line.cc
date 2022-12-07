#include <iostream>

#include "cache.h"

void CACHE::prefetcher_initialize() { std::cout << NAME << " next line prefetcher" << std::endl; }

uint32_t CACHE::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)
{
  auto pf_addr = addr + BLOCK_SIZE;
  prefetch_line(pf_addr, true, metadata_in);
  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(champsim::address addr, uint32_t set, uint32_t way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}

void CACHE::prefetcher_final_stats() {}
