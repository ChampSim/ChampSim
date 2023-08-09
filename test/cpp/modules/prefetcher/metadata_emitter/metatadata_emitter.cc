#include "cache.h"

#include <map>
#include <vector>

namespace test
{
  std::map<CACHE*, uint32_t> metadata_operate_emitter;
  std::map<CACHE*, uint32_t> metadata_fill_emitter;
}

void CACHE::prefetcher_initialize() {}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in) {
  auto found = test::metadata_operate_emitter.find(this);
  if (found != std::end(test::metadata_operate_emitter))
    return found->second;
  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  auto found = test::metadata_fill_emitter.find(this);
  if (found != std::end(test::metadata_fill_emitter))
    return found->second;
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}

void CACHE::prefetcher_final_stats() {}


