#include "drrip.h"

#include <algorithm>
#include <cassert>
#include <random>
#include <utility>

#include "champsim.h"

drrip::drrip(CACHE* cache) : replacement(cache), NUM_SET(cache->NUM_SET), NUM_WAY(cache->NUM_WAY), rrpv(static_cast<std::size_t>(NUM_SET * NUM_WAY))
{
  // randomly selected sampler sets
  std::size_t TOTAL_SDM_SETS = NUM_CPUS * NUM_POLICY * SDM_SIZE;
  std::generate_n(std::back_inserter(rand_sets), TOTAL_SDM_SETS, std::knuth_b{1});
  std::sort(std::begin(rand_sets), std::end(rand_sets));
  std::fill_n(std::back_inserter(PSEL), NUM_CPUS, typename decltype(PSEL)::value_type{0});
}

unsigned& drrip::get_rrpv(long set, long way) { return rrpv.at(static_cast<std::size_t>(set * NUM_WAY + way)); }

void drrip::update_bip(long set, long way)
{
  get_rrpv(set, way) = maxRRPV;

  bip_counter++;
  if (bip_counter == BIP_MAX) {
    bip_counter = 0;
    get_rrpv(set, way) = maxRRPV - 1;
  }
}

void drrip::update_srrip(long set, long way) { get_rrpv(set, way) = maxRRPV - 1; }

// called on every cache hit and cache fill
void drrip::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                     champsim::address victim_addr, access_type type, uint8_t hit)
{
  // do not update replacement state for writebacks
  if (access_type{type} == access_type::WRITE) {
    get_rrpv(set, way) = maxRRPV - 1;
    return;
  }

  // cache hit
  if (hit) {
    get_rrpv(set, way) = 0; // for cache hit, DRRIP always promotes a cache line to the MRU position
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
