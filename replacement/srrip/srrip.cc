#include "srrip.h"

#include <algorithm>
#include <cassert>
#include <unordered_map>

#include "cache.h"

champsim::modules::replacement::register_module<srrip> srrip_register("srrip");

void srrip::initialize_replacement() {
  std::generate_n(std::back_inserter(sets), intern_->NUM_SET, [cache = intern_, ways = intern_->NUM_WAY] {return srrip_set_helper{cache->NUM_WAY};});
}

// find replacement victim
long srrip::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                        champsim::address full_addr, access_type type)
{
  return sets.at(static_cast<std::size_t>(set)).victim();
}

// called on every cache hit and cache fill
void srrip::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                     champsim::address victim_addr, access_type type, bool hit)
{
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
