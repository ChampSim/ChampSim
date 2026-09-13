#include "drrip.h"

#include <algorithm>
#include <cassert>
#include <random>
#include <utility>

#include "champsim.h"

champsim::modules::replacement::register_module<drrip> drrip_register("drrip");

drrip::drrip(champsim::modules::ModuleBuilder builder)
    : NUM_SET(builder.get_parent<champsim::modules::cache_module>()->num_sets()), NUM_WAY(builder.get_parent<champsim::modules::cache_module>()->num_ways()),
      rrpv(static_cast<std::size_t>(NUM_SET * NUM_WAY)), PSEL(builder.get_parameter<std::size_t>("num_consumers", true, std::size_t{1}),
                                                              champsim::msl::dscounter<long, PSEL_WIDTH>(champsim::msl::get_sample_rate(NUM_SET)))
{
}

void drrip::initialize_replacement() {}

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
void drrip::update_replacement_state(champsim::origin origin, long set, long way, champsim::address full_addr, champsim::address ip,
                                     champsim::address victim_addr, access_type type, bool hit)
{

  // cache hit
  if (hit) {
    // do not update replacement state for writebacks
    if (access_type{type} == access_type::WRITE) {
      get_rrpv(set, way) = maxRRPV - 1;
      return;
    }
    get_rrpv(set, way) = 0; // for cache hit, DRRIP always promotes a cache line to the MRU position
  }
}

void drrip::replacement_cache_fill(champsim::origin origin, long set, long way, champsim::address full_addr, champsim::address ip,
                                   champsim::address victim_addr, access_type type)
{
  // do not update replacement state for writebacks
  if (access_type{type} == access_type::WRITE) {
    get_rrpv(set, way) = maxRRPV - 1;
    return;
  }
  if (PSEL[origin.cpu()].decide(set)) {
    update_brrip(set, way);
  } else {
    update_srrip(set, way);
  }
  // cache miss, update bad
  PSEL[origin.cpu()].update_bad(set);
}

// find replacement victim
long drrip::find_victim(champsim::origin origin, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
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
