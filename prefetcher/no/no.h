#ifndef PREFETCHER_NO_H
#define PREFETCHER_NO_H

#include <cstdint>

#include "champsim.h"
#include "modules.h"

struct no : champsim::modules::prefetcher
{
  using prefetcher::prefetcher;

  //void prefetcher_initialize() {}
  //void prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target) {}
  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in);
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in);
  //void prefetcher_cycle_operate() {}
  //void prefetcher_final_stats() {}
};

#endif
