#include "cache.h"

#include <algorithm>
#include <iterator>
#include "champsim_constants.h"
#include "util.h"

uint32_t CACHE::lru_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK *current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
    return std::distance(current_set, std::max_element(current_set, std::next(current_set, NUM_WAY), lru_comparator<BLOCK, BLOCK>()));
}

void CACHE::lru_update(uint32_t set, uint32_t way, uint32_t type, uint8_t hit)
{
    if (hit && type == WRITEBACK)
        return;

    auto begin = std::next(block.begin(), set*NUM_WAY);
    auto end   = std::next(begin, NUM_WAY);
    uint32_t hit_lru = std::next(begin, way)->lru;
    std::for_each(begin, end, [hit_lru](BLOCK &x){ if (x.lru <= hit_lru) x.lru++; });
    std::next(begin, way)->lru = 0; // promote to the MRU position
}

void CACHE::lru_final_stats()
{
}

