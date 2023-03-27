#include "lru.h"

#include <algorithm>

uint32_t lru::find_victim(uint32_t triggering_cpu, uint64_t instr_id, uint32_t set, const CACHE::BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
  auto begin = std::next(std::begin(last_used_cycles), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);

  // Find the way whose last use cycle is most distant
  auto victim = std::min_element(begin, end);
  assert(begin <= victim);
  assert(victim < end);
  return static_cast<uint32_t>(std::distance(begin, victim)); // cast protected by prior asserts
}

void lru::update_replacement_state(uint32_t triggering_cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit)
{
  // Mark the way as being used on the current cycle
  if (!hit || type != WRITE) // Skip this for writeback hits
    last_used_cycles.at(set * NUM_WAY + way) = cycle++;
}

