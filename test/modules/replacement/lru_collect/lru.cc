#include <algorithm>
#include <map>
#include <vector>

#include "cache.h"
#include "../../../repl_interface.h"

namespace
{
std::map<CACHE*, std::vector<uint64_t>> last_used_cycles;
}

namespace test
{
  std::map<CACHE*, std::vector<repl_update_interface>> replacement_update_state_collector;
}

void CACHE::initialize_replacement() { ::last_used_cycles.insert_or_assign(this, std::vector<uint64_t>(NUM_SET * NUM_WAY)); }

uint32_t CACHE::find_victim(uint32_t triggering_cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
  auto begin = std::next(std::begin(::last_used_cycles.at(this)), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);

  // Find the way whose last use cycle is most distant
  return static_cast<uint32_t>(std::distance(begin, std::min_element(begin, end)));
}

void CACHE::update_replacement_state(uint32_t triggering_cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type,
                                     uint8_t hit)
{
  test::replacement_update_state_collector.try_emplace(this);
  test::replacement_update_state_collector.at(this).push_back({cpu, set, way, full_addr, ip, victim_addr, type, hit});

  // Mark the way as being used on the current cycle
  if (!hit || type != WRITE) // Skip this for writeback hits
    ::last_used_cycles.at(this).at(set * NUM_WAY + way) = current_cycle;
}

void CACHE::replacement_final_stats() {}
