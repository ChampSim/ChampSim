#include "drrip.h"
#include <algorithm>
#include <cassert>
#include <utility>

drrip::drrip(CACHE* cache) : replacement(cache), NUM_SET(cache->NUM_SET), NUM_WAY(cache->NUM_WAY), rrpv(NUM_SET * NUM_WAY)
{
  // randomly selected sampler sets
  std::size_t rand_seed = 1103515245 + 12345;
  for (std::size_t i = 0; i < TOTAL_SDM_SETS; i++) {
    std::size_t val = (rand_seed / 65536) % NUM_SET;
    auto loc = std::lower_bound(std::begin(rand_sets), std::end(rand_sets), val);

    while (loc != std::end(rand_sets) && *loc == val) {
      rand_seed = rand_seed * 1103515245 + 12345;
      val = (rand_seed / 65536) % NUM_SET;
      loc = std::lower_bound(std::begin(rand_sets), std::end(rand_sets), val);
    }

    rand_sets.insert(loc, val);
  }
}

void drrip::update_bip(long set, long way)
{
  rrpv[set * NUM_WAY + way] = maxRRPV;

  bip_counter++;
  if (bip_counter == BIP_MAX) {
    bip_counter = 0;
    rrpv[set * NUM_WAY + way] = maxRRPV - 1;
  }
}

void drrip::update_srrip(long set, long way)
{
  rrpv[set * NUM_WAY + way] = maxRRPV - 1;
}

// called on every cache hit and cache fill
void drrip::update_replacement_state(uint32_t triggering_cpu, long set, long way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, access_type type,
                                     uint8_t hit)
{
  // do not update replacement state for writebacks
  if (access_type{type} == access_type::WRITE) {
    rrpv[set * NUM_WAY + way] = maxRRPV - 1;
    return;
  }

  // cache hit
  if (hit) {
    rrpv[set * NUM_WAY + way] = 0; // for cache hit, DRRIP always promotes a cache line to the MRU position
    return;
  }

  // cache miss
  auto begin = std::next(std::begin(rand_sets), triggering_cpu * NUM_POLICY * SDM_SIZE);
  auto end = std::next(begin, NUM_POLICY * SDM_SIZE);
  auto leader = std::find(begin, end, set);

  if (leader == end) { // follower sets
    auto selector = PSEL[triggering_cpu];
    if (selector.value() > (selector.maximum / 2)) { // follow BIP
      update_bip(set, way);
    } else { // follow SRRIP
      update_srrip(set, way);
    }
  } else if (leader == begin) { // leader 0: BIP
    PSEL[triggering_cpu]--;
    update_bip(set, way);
  } else if (leader == std::next(begin)) { // leader 1: SRRIP
    PSEL[triggering_cpu]++;
    update_srrip(set, way);
  }
}

// find replacement victim
long drrip::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const CACHE::BLOCK* current_set, uint64_t ip, uint64_t full_addr, access_type type)
{
  // look for the maxRRPV line
  auto begin = std::next(std::begin(rrpv), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);

  auto victim = std::max_element(begin, end);
  for (auto it = begin; it != end; ++it)
    *it += maxRRPV - *victim;

  assert(begin <= victim);
  assert(victim < end);
  return std::distance(begin, victim); // cast protected by assertions
}
