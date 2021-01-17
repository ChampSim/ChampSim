#include "cache.h"

void CACHE::l1d_prefetcher_initialize() 
{
    cout << "CPU " << cpu << " L1D virtual address space next line prefetcher" << endl;
}

void CACHE::l1d_prefetcher_operate(uint64_t v_addr, uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type)
{
  uint64_t page_offset = ((addr>>LOG2_BLOCK_SIZE)&63);

  if(page_offset < 63)
    {
      uint64_t pf_addr = ((addr>>LOG2_BLOCK_SIZE)+1) << LOG2_BLOCK_SIZE;
      prefetch_line(ip, addr, pf_addr, FILL_L1, 0);      
    }
  else
    {
      uint64_t pf_addr = ((v_addr>>LOG2_BLOCK_SIZE)+1) << LOG2_BLOCK_SIZE;
      va_prefetch_line(ip, pf_addr, FILL_L1, 0);
    }
}

void CACHE::l1d_prefetcher_cache_fill(uint64_t v_addr, uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{

}

void CACHE::l1d_prefetcher_final_stats()
{
    cout << "CPU " << cpu << " L1D virtual address space next line prefetcher final stats" << endl;
}
