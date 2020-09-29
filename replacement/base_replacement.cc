#include "cache.h"

#include <algorithm>
#include <iterator>

class lru_comparator
{
    public:
        bool operator()(const BLOCK &lhs, const BLOCK &rhs)
        {
            return lhs.lru < rhs.lru;
        }
};

uint32_t CACHE::lru_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK *current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
    return std::distance(current_set, std::max_element(current_set, std::next(current_set, NUM_WAY), lru_comparator()));
}

void CACHE::lru_update(uint32_t set, uint32_t way, uint32_t type, uint8_t hit)
{
    if (hit && type == WRITEBACK)
        return;

    uint32_t hit_lru = block[set][way].lru;
    std::for_each(block[set], std::next(block[set], NUM_WAY), [hit_lru](BLOCK &x){ if (x.lru <= hit_lru) x.lru++; });
    block[set][way].lru = 0; // promote to the MRU position
}

void CACHE::lru_final_stats()
{
}

