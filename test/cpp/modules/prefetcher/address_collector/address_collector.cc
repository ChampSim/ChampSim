#include "cache.h"

#include <map>
#include <vector>

namespace test
{
  std::map<CACHE*, std::vector<uint64_t>> address_operate_collector;
  std::map<CACHE*, std::vector<uint64_t>> address_fill_collector;
}

void CACHE::prefetcher_initialize() {}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  test::address_operate_collector[this].push_back(addr);
  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  test::address_fill_collector[this].push_back(addr);
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}

void CACHE::prefetcher_final_stats() {}

