#include "cache.h"

#include <string>

// initialize replacement state
void CACHE::llc_initialize_replacement()
{

}

// find replacement victim
uint32_t CACHE::llc_find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK *current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
    // baseline LRU
    return lru_victim(cpu, instr_id, set, current_set, ip, full_addr, type); 
}

// called on every cache hit and cache fill
void CACHE::llc_update_replacement_state(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit)
{
    std::string TYPE_NAME;
    if (type == LOAD)
        TYPE_NAME = "LOAD";
    else if (type == RFO)
        TYPE_NAME = "RFO";
    else if (type == PREFETCH)
        TYPE_NAME = "PF";
    else if (type == WRITEBACK)
        TYPE_NAME = "WB";
    else if (type == TRANSLATION)
		TYPE_NAME = "TRANSLATION";
	else
        assert(0);

    if (hit)
        TYPE_NAME += "_HIT";
    else
        TYPE_NAME += "_MISS";

    if ((type == WRITEBACK) && ip)
        assert(0);

    // uncomment this line to see the LLC accesses
    // std::cout << "CPU: " << cpu << "  LLC " << std::setw(9) << TYPE_NAME << " set: " << std::setw(5) << set << " way: " << std::setw(2) << way;
    // std::cout << std::hex << " paddr: " << std::setw(12) << paddr << " ip: " << std::setw(8) << ip << " victim_addr: " << victim_addr << std::dec << std::endl;

    return lru_update(set, way, type, hit);
}

void CACHE::llc_replacement_final_stats()
{

}
