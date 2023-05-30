#include "cache.h"

#include <map>
#include <vector>

#include "../../../src/pref_interface.h"

namespace test
{
  std::map<CACHE*, std::vector<pref_cache_operate_interface>> prefetch_hit_collector;
}

void CACHE::prefetcher_initialize() {}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  test::prefetch_hit_collector[this].push_back({addr, ip, cache_hit, useful_prefetch, type, metadata_in});
  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}

void CACHE::prefetcher_final_stats() {}

