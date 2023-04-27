#include "srrip.h"

#include <algorithm>

#include "cache.h"

srrip::srrip(CACHE* cache) : replacement(cache), NUM_SET(cache->NUM_SET), NUM_WAY(cache->NUM_WAY), rrpv_values(NUM_SET*NUM_WAY, maxRRPV) {}

// find replacement victim
long srrip::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const CACHE::BLOCK* current_set, champsim::address ip, champsim::address full_addr, uint32_t type)
{
  // look for the maxRRPV line
  auto begin = std::next(std::begin(rrpv_values), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);
  auto victim = std::find(begin, end, maxRRPV);
  while (victim == end) {
    for (auto it = begin; it != end; ++it)
      ++(*it);

    victim = std::find(begin, end, maxRRPV);
  }

  assert(begin <= victim);
  assert(victim < end);
  return std::distance(begin, victim); // cast protected by assertions
}

// called on every cache hit and cache fill
void srrip::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr, uint32_t type,
                                     uint8_t hit)
{
  if (hit)
    rrpv_values[set * NUM_WAY + way] = 0;
  else
    rrpv_values[set * NUM_WAY + way] = maxRRPV - 1;
}
