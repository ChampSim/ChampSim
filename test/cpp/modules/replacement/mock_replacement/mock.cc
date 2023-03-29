#include <algorithm>
#include <map>
#include <vector>

#include "cache.h"
#include "../../../src/repl_interface.h"

namespace test
{
  std::map<CACHE*, uint32_t> evict_way;
}

void CACHE::initialize_replacement() {}

uint32_t CACHE::find_victim(uint32_t triggering_cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, champsim::address ip, champsim::address full_addr, uint32_t type)
{
  // Find the way whose last use cycle is most distant
  auto evict_way_it = test::evict_way.try_emplace(this, 0);
  return evict_way_it.first->second;
}

void CACHE::update_replacement_state(uint32_t triggering_cpu, uint32_t set, uint32_t way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr, uint32_t type,
                                     uint8_t hit)
{
}

void CACHE::replacement_final_stats() {}
