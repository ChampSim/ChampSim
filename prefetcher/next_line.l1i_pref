#include "ooo_cpu.h"

void O3_CPU::l1i_prefetcher_initialize() 
{
  cout << "CPU " << cpu << " L1I next line prefetcher" << endl;
}

void O3_CPU::l1i_prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target)
{
  
}

void O3_CPU::l1i_prefetcher_cache_operate(uint64_t v_addr, uint8_t cache_hit, uint8_t prefetch_hit)
{
  //cout << "access v_addr: 0x" << hex << v_addr << dec << endl;
  
  if((cache_hit == 0) && (L1I.MSHR.occupancy < (L1I.MSHR.SIZE>>1)))
    {
      uint64_t pf_addr = v_addr + (1<<LOG2_BLOCK_SIZE);
      prefetch_code_line(pf_addr);
    }
}

void O3_CPU::l1i_prefetcher_cycle_operate()
{

}

void O3_CPU::l1i_prefetcher_cache_fill(uint64_t v_addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_v_addr)
{
  //cout << hex << "fill: 0x" << v_addr << dec << " " << set << " " << way << " " << (uint32_t)prefetch << " " << hex << "evict: 0x" << evicted_v_addr << dec << endl;
}

void O3_CPU::l1i_prefetcher_final_stats()
{
  cout << "CPU " << cpu << " L1I next line prefetcher final stats" << endl;
}
