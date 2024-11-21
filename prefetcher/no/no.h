#ifndef PREFETCHER_NO_H
#define PREFETCHER_NO_H

#include <cstdint>

#include "champsim.h"
#include "modules.h"

class no : public champsim::modules::prefetcher
{
public:

  void prefetcher_initialize() {}
  void prefetcher_branch_operate(champsim::address ip, uint8_t branch_type, champsim::address branch_target) {(void)ip; (void)branch_type; (void)branch_target;}
  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, bool cache_hit, bool useful_prefetch, access_type type,
                                    uint32_t metadata_in);
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, bool prefetch, champsim::address evicted_addr, uint32_t metadata_in);
  void prefetcher_cycle_operate() {}
  void prefetcher_final_stats() {}
};



#endif
