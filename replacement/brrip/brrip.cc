#include "brrip.h"

#include <algorithm>
#include <fmt/core.h>
#include <iterator>

brrip::brrip(CACHE* cache) : brrip(cache, cache->NUM_SET, cache->NUM_WAY) {}

brrip::brrip(CACHE* cache, long sets_, long ways_) : replacement(cache)
{
  std::generate_n(std::back_inserter(sets), sets_, [ways = ways_] { return brrip_set_helper{ways}; });
}

long brrip::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                        champsim::address full_addr, access_type type)
{
  return sets.at(static_cast<std::size_t>(set)).victim();
}

void brrip::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                     champsim::address victim_addr, access_type type, uint8_t hit)
{
  if (hit)
    sets.at(static_cast<std::size_t>(set)).update(way, true, brrip_set_helper::maxRRPV);
}

void brrip::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type)
{
  ++fills;

  auto insertion_rrpv = brrip_set_helper::maxRRPV;
  if (type == access_type::WRITE) {
    ++writeback_fills;
    insertion_rrpv = brrip_set_helper::maxRRPV - 1;
    ++writeback_rrpv2_insertions;
  } else {
    ++eligible_fills;
    ++fill_counter;
    if (fill_counter % BRRIP_MAX == 0)
      insertion_rrpv = brrip_set_helper::maxRRPV - 1;

    if (insertion_rrpv == brrip_set_helper::maxRRPV)
      ++rrpv3_insertions;
    else
      ++rrpv2_insertions;
  }

  sets.at(static_cast<std::size_t>(set)).update(way, false, insertion_rrpv);
}

void brrip::replacement_final_stats()
{

}

brrip_set_helper::brrip_set_helper(long ways) : rrpv_values(static_cast<std::size_t>(ways), maxRRPV) {}

auto brrip_set_helper::get_rrpv(long way) -> rrpv_type& { return rrpv_values.at(static_cast<std::size_t>(way)); }

long brrip_set_helper::victim()
{
  auto victim = std::max_element(std::begin(rrpv_values), std::end(rrpv_values));

  std::transform(std::cbegin(rrpv_values), std::cend(rrpv_values), std::begin(rrpv_values), [diff = maxRRPV - *victim](auto x) { return x + diff; });

  return std::distance(std::begin(rrpv_values), victim);
}

void brrip_set_helper::update(long way, bool hit, rrpv_type insertion_rrpv) { get_rrpv(way) = hit ? 0 : insertion_rrpv; }
