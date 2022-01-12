#include "cache.h"

void CACHE::l2c_prefetcher_initialize() 
{
    std::cout << "CPU " << cpu << " L2C next line prefetcher" << std::endl;
}

uint32_t CACHE::l2c_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)
{
    uint64_t pf_addr = ((addr>>LOG2_BLOCK_SIZE)+1) << LOG2_BLOCK_SIZE;

    DP ( if (warmup_complete[cpu]) {
            std::cout << "[" << NAME << "] " << __func__ << std::hex << " base_cl: " << (addr>>LOG2_BLOCK_SIZE);
            std::cout << " pf_cl: " << (pf_addr>>LOG2_BLOCK_SIZE) << " ip: " << ip << " cache_hit: " << +cache_hit << " type: " << +type << std::endl; });

    prefetch_line(ip, addr, pf_addr, FILL_L2, 0);

    return metadata_in;
}

uint32_t CACHE::l2c_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::l2c_prefetcher_final_stats()
{
    std::cout << "CPU " << cpu << " L2C next line prefetcher final stats" << std::endl;
}
