#include "cache.h"

#include <iostream>

void CACHE::prefetcher_initialize()
{
    std::cout << NAME << " next line prefetcher" << std::endl;
}

void CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
    uint64_t pf_addr = addr + (1 << LOG2_BLOCK_SIZE);
    prefetch_line(ip, addr, pf_addr, FILL_L1, 0);
}

void CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
}

void CACHE::prefetcher_cycle_operate()
{
}

void CACHE::prefetcher_final_stats()
{
}

