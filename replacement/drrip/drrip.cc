#include "drrip.h"

#include <algorithm>
#include <cassert>
#include <random>
#include <utility>

#include "champsim.h"

drrip::drrip(CACHE* cache) : replacement(cache), NUM_SET(cache->NUM_SET), NUM_WAY(cache->NUM_WAY), rrpv(static_cast<std::size_t>(NUM_SET * NUM_WAY))
{
  std::fill_n(std::back_inserter(PSEL), NUM_CPUS, typename decltype(PSEL)::value_type{1 << (PSEL_WIDTH - 1)});
}

unsigned& drrip::get_rrpv(long set, long way) { return rrpv.at(static_cast<std::size_t>(set * NUM_WAY + way)); }

void drrip::update_brrip(long set, long way)
{
  get_rrpv(set, way) = maxRRPV;

  brrip_counter++;
  if (brrip_counter == BRRIP_MAX) {
    brrip_counter = 0;
    get_rrpv(set, way) = maxRRPV - 1;
  }
}

void drrip::update_srrip(long set, long way) { get_rrpv(set, way) = maxRRPV - 1; }

// called on every cache hit and cache fill
void drrip::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                     champsim::address victim_addr, access_type type, uint8_t hit)
{

  // cache hit
  if(hit) {
    // do not update replacement state for writebacks
    if (access_type{type} == access_type::WRITE) {
      get_rrpv(set, way) = maxRRPV - 1;
      return;
    }
    get_rrpv(set, way) = 0; // for cache hit, DRRIP always promotes a cache line to the MRU position
  }
}

void drrip::replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr, access_type type)
{
  // do not update replacement state for writebacks
  if (access_type{type} == access_type::WRITE) {
    get_rrpv(set, way) = maxRRPV - 1;
    return;
  }
  // cache miss
  auto selector = PSEL[triggering_cpu];
  switch(get_set_type(set)) {
    case set_type::follower:
      if (selector.value() > (selector.maximum / 2)) { // follow BRRIP
        update_brrip(set, way);
      } else { // follow SRRIP
        update_srrip(set, way);
      }
      break;
    case set_type::brrip_leader:
      PSEL[triggering_cpu]--;
      update_brrip(set, way);
      break;
    case set_type::srrip_leader:
      PSEL[triggering_cpu]++;
      update_srrip(set, way);
      break;
  }
}

// find replacement victim
long drrip::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                        champsim::address full_addr, access_type type)
{
  // look for the maxRRPV line
  auto begin = std::next(std::begin(rrpv), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);

  auto victim = std::max_element(begin, end);
  if (auto rrpv_update = maxRRPV - *victim; rrpv_update != 0)
    for (auto it = begin; it != end; ++it)
      *it += rrpv_update;

  assert(begin <= victim);
  assert(victim < end);
  return std::distance(begin, victim); // cast protected by assertions
}
