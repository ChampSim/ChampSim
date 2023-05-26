#ifndef PREFETCHER_NEXT_LINE_H
#define PREFETCHER_NEXT_LINE_H

#include "address.h"
#include "modules.h"
#include <cstdint>

struct next_line : champsim::modules::prefetcher
{
  using prefetcher::prefetcher;
  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in);
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in);

  //void prefetcher_initialize();
  //void prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target) {}
  //void prefetcher_cycle_operate() {}
  //void prefetcher_final_stats() {}
};

#endif
