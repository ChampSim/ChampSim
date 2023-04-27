#include "next_line.h"
#include <iostream>

#include "cache.h"

void next_line::prefetcher_initialize() { std::cout << intern_->NAME << " next line prefetcher" << std::endl; }

uint32_t next_line::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)
{
  champsim::block_number pf_addr{addr};
  prefetch_line(champsim::address{pf_addr+1}, true, metadata_in);
  return metadata_in;
}

uint32_t next_line::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}
