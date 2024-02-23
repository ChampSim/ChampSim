#include "next_line.h"

uint32_t next_line::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                             uint32_t metadata_in)
{
  champsim::block_number pf_addr{addr};
  prefetch_line(champsim::address{pf_addr + 1}, true, metadata_in);
  return metadata_in;
}

uint32_t next_line::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}
