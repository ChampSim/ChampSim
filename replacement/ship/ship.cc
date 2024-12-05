#include "ship.h"

#include <algorithm>
#include <cassert>
#include <random>

#include "champsim.h"

// initialize replacement state
ship::ship(CACHE* cache)
    : replacement(cache), NUM_SET(cache->NUM_SET), NUM_WAY(cache->NUM_WAY), sampler(SAMPLER_SET_FACTOR * NUM_CPUS * static_cast<std::size_t>(NUM_WAY)),
      rrpv_values(static_cast<std::size_t>(NUM_SET * NUM_WAY), maxRRPV)
{
  // randomly selected sampler sets
  std::generate_n(std::back_inserter(rand_sets), SAMPLER_SET_FACTOR * NUM_CPUS, std::knuth_b{1});
  std::sort(std::begin(rand_sets), std::end(rand_sets));

  std::generate_n(std::back_inserter(SHCT), NUM_CPUS, []() -> typename decltype(SHCT)::value_type { return {}; });
}

int& ship::get_rrpv(long set, long way) { return rrpv_values.at(static_cast<std::size_t>(set * NUM_WAY + way)); }

// find replacement victim
long ship::find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                       champsim::address full_addr, access_type type)
{
  // look for the maxRRPV line
  auto begin = std::next(std::begin(rrpv_values), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);

  auto victim = std::max_element(begin, end);
  if (auto rrpv_update = maxRRPV - *victim; rrpv_update != 0)
    for (auto it = begin; it != end; ++it)
      *it += rrpv_update;

  assert(begin <= victim);
  assert(victim < end);
  return std::distance(begin, victim);
}

// called on every cache hit and cache fill
void ship::update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                    champsim::address victim_addr, access_type type, uint8_t hit)
{
  using namespace champsim::data::data_literals;
  // handle writeback access
  if (access_type{type} == access_type::WRITE) {
    if (!hit)
      get_rrpv(set, way) = maxRRPV - 1;

    return;
  }

  // update sampler
  auto s_idx = std::find(std::begin(rand_sets), std::end(rand_sets), set);
  if (s_idx != std::end(rand_sets)) {
    auto s_set_begin = std::next(std::begin(sampler), std::distance(std::begin(rand_sets), s_idx));
    auto s_set_end = std::next(s_set_begin, NUM_WAY);

    // check hit
    auto match = std::find_if(s_set_begin, s_set_end, [addr = full_addr, shamt = champsim::data::bits{8 + champsim::lg2(NUM_WAY)}](auto x) {
      return x.valid && x.address.slice_upper(shamt) == addr.slice_upper(shamt);
    });
    if (match != s_set_end) {
      auto SHCT_idx = match->ip.slice_lower<32_b>().to<std::size_t>() % SHCT_PRIME;
      SHCT[triggering_cpu][SHCT_idx]--;

      match->used = true;
    } else {
      match = std::min_element(s_set_begin, s_set_end, [](auto x, auto y) { return x.last_used < y.last_used; });

      if (match->used) {
        auto SHCT_idx = match->ip.slice_lower<32_b>().to<std::size_t>() % SHCT_PRIME;
        SHCT[triggering_cpu][SHCT_idx]++;
      }

      match->valid = true;
      match->address = full_addr;
      match->ip = ip;
      match->used = false;
    }

    // update LRU state
    match->last_used = access_count++;
  }

  if (hit)
    get_rrpv(set, way) = 0;
  else {
    // SHIP prediction
    auto SHCT_idx = ip.slice_lower<32_b>().to<std::size_t>() % SHCT_PRIME;

    get_rrpv(set, way) = maxRRPV - 1;
    if (SHCT[triggering_cpu][SHCT_idx].is_max())
      get_rrpv(set, way) = maxRRPV;
  }
}
