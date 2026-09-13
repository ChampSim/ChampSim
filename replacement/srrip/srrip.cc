#include "srrip.h"

#include <algorithm>
#include <cassert>
#include <unordered_map>

#include "cache.h"

champsim::modules::replacement::register_module<srrip> srrip_register("srrip");

srrip::srrip(champsim::modules::ModuleBuilder builder)
    : srrip(builder.get_parent<champsim::modules::cache_module>(), builder.get_parent<champsim::modules::cache_module>()->num_sets(),
            builder.get_parent<champsim::modules::cache_module>()->num_ways())
{
}

srrip::srrip(champsim::modules::cache_module* cache, long sets_, long ways_)
{
  std::generate_n(std::back_inserter(sets), sets_, [ways = ways_] { return srrip_set_helper{ways}; });
}

// find replacement victim
long srrip::find_victim(champsim::origin origin, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                        champsim::address full_addr, access_type type)
{
  return sets.at(static_cast<std::size_t>(set)).victim();
}

// called on every cache hit and cache miss
void srrip::update_replacement_state(champsim::origin origin, long set, long way, champsim::address full_addr, champsim::address ip,
                                     champsim::address victim_addr, access_type type, bool hit)
{
  // On a miss, way is past-the-end and not valid for indexing
  if (hit)
    sets.at(static_cast<std::size_t>(set)).update(way, hit);
}

srrip_set_helper::srrip_set_helper(long ways) : rrpv_values(static_cast<std::size_t>(ways), maxRRPV) {}

auto srrip_set_helper::get_rrpv(long way) -> rrpv_type& { return rrpv_values.at(static_cast<std::size_t>(way)); }

long srrip_set_helper::victim()
{
  // Find the maximum RRPV
  auto victim = std::max_element(std::begin(rrpv_values), std::end(rrpv_values));

  // If the maximum element has RRPV less than the maximum, increment everything to the maximum
  std::transform(std::cbegin(rrpv_values), std::cend(rrpv_values), std::begin(rrpv_values), [diff = maxRRPV - *victim](auto x) { return x + diff; });

  // Return the way index
  return std::distance(std::begin(rrpv_values), victim);
}

void srrip_set_helper::update(long way, bool hit) { get_rrpv(way) = hit ? 0 : (maxRRPV - 1); }
