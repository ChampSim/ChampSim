#include "cache.h"

#include <map>
#include <vector>

namespace test
{
  std::map<CACHE*, std::vector<uint32_t>> metadata_operate_collector;
  std::map<CACHE*, std::vector<uint32_t>> metadata_fill_collector;
}

void CACHE::prefetcher_initialize() {}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in) {
  auto it = test::metadata_operate_collector.try_emplace(this);
  it.first->second.push_back(metadata_in);
  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  auto it = test::metadata_fill_collector.try_emplace(this);
  it.first->second.push_back(metadata_in);
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}

void CACHE::prefetcher_final_stats() {}

