#include "no.h"

champsim::modules::prefetcher::register_module<no> no_register("no");
uint32_t no::prefetcher_cache_operate(champsim::address addr, champsim::address ip, bool cache_hit, bool useful_prefetch, access_type type,
                                      uint32_t metadata_in)
{
  // assert(addr == ip); // Invariant for instruction prefetchers
  return metadata_in;
}

uint32_t no::prefetcher_cache_fill(champsim::address addr, long set, long way, bool prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}
