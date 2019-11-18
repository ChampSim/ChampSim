#include "ooo_cpu.h"

void O3_CPU::l1i_prefetcher_initialize() 
{

}

void O3_CPU::l1i_prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target)
{
  if(branch_target != 0)
    {
      prefetch_code_line(ip, branch_target);
    }
}

void O3_CPU::l1i_prefetcher_cache_operate(uint64_t addr, uint8_t cache_hit)
{
  if((cache_hit == 0) && (L1I.MSHR.occupancy < (L1I.MSHR.SIZE>>1)))
    {
      uint64_t pf_addr = addr + (1<<LOG2_BLOCK_SIZE);
      prefetch_code_line(addr, pf_addr);
    }
}

void O3_CPU::l1i_prefetcher_final_stats()
{

}
