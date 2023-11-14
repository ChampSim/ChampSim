#include "srrip.h"

#include <algorithm>
#include <cassert>
#include <unordered_map>

#include "cache.h"

srrip::srrip(CACHE* cache) : srrip(cache, cache->NUM_SET, cache->NUM_WAY) {}

srrip::srrip(CACHE* cache, long sets_, long ways_) : replacement(cache)
{
  std::generate_n(std::back_inserter(sets), sets_, [ways = ways_] { return srrip_set_helper{ways}; });
}

// find replacement victim
long srrip::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const CACHE::BLOCK* current_set, uint64_t ip, uint64_t full_addr,
                        access_type type)
{
  return sets.at(set).victim();
}

// called on every cache hit and cache fill
void srrip::update_replacement_state(uint32_t triggering_cpu, long set, long way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, access_type type,
                                     uint8_t hit)
{
  sets.at(set).update(way, hit);
}

srrip_set_helper::srrip_set_helper(long ways) : rrpv_values(static_cast<std::size_t>(ways), maxRRPV) {}

auto srrip_set_helper::get_rrpv(long way) -> rrpv_type& { return rrpv_values.at(static_cast<std::size_t>(way)); }

long srrip_set_helper::victim()
{
  // Find the maximum RRPV
  auto victim = std::max_element(std::begin(rrpv_values), std::end(rrpv_values));

  // If the maximum element has RRPV less than the maximum, increment everything to the maximum
  std::for_each(std::begin(rrpv_values), std::end(rrpv_values), [diff = maxRRPV - *victim](auto& x) { x += diff; });

  // Return the way index
  return std::distance(std::begin(rrpv_values), victim);
}

void srrip_set_helper::update(long way, bool hit) { get_rrpv(way) = hit ? 0 : (maxRRPV - 1); }
