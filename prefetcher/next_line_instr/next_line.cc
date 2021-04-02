#include "ooo_cpu.h"

#include <iostream>

void O3_CPU::prefetcher_initialize()
{
    std::cout << NAME << " next line instruction prefetcher" << endl;
}

void O3_CPU::prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target)
{
}

void O3_CPU::prefetcher_cache_operate(uint64_t v_addr, uint8_t cache_hit, uint8_t prefetch_hit)
{
    uint64_t pf_addr = v_addr + (1<<LOG2_BLOCK_SIZE);
    prefetch_code_line(pf_addr);
}

void O3_CPU::prefetcher_cycle_operate()
{
}

void O3_CPU::prefetcher_cache_fill(uint64_t v_addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_v_addr)
{
}

void O3_CPU::l1i_prefetcher_final_stats()
{
}

