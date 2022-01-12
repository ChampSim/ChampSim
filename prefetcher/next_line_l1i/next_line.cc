#include "ooo_cpu.h"

void O3_CPU::l1i_prefetcher_initialize() 
{
    std::cout << "CPU " << cpu << " L1I next line prefetcher" << std::endl;
}

void O3_CPU::l1i_prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target)
{
  
}

void O3_CPU::l1i_prefetcher_cache_operate(uint64_t v_addr, uint8_t cache_hit, uint8_t prefetch_hit)
{
  //std::cout << "access v_addr: 0x" << std::hex << v_addr << std::dec << std::endl;
  
  if((cache_hit == 0) && (L1I.get_occupancy(0,0) < (L1I.get_size(0,0)>>1)))
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
  //std::cout << std::hex << "fill: 0x" << v_addr << std::dec << " " << set << " " << way << " " << (uint32_t)prefetch << " " << std::hex << "evict: 0x" << evicted_v_addr << std::dec << std::endl;
}

void O3_CPU::l1i_prefetcher_final_stats()
{
    std::cout << "CPU " << cpu << " L1I next line prefetcher final stats" << std::endl;
}
