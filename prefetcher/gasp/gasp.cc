#include <algorithm>
#include <array>
#include <map>
#include <optional>

#include "global.hpp"
#include "cache.h"
#include "msl/lru_table.h"

#include "gasp.h"

GASP prefetcher;

void CACHE::prefetcher_initialize() {
  prefetcher = GASP();
}

void CACHE::prefetcher_cycle_operate() { prefetcher.advance_lookahead(this); }

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, uint8_t type, uint32_t metadata_in)
{
  prefetcher.initiate_lookahead(ip, addr >> LOG2_BLOCK_SIZE);
  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_final_stats() {}
