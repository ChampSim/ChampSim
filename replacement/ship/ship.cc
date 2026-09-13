#include "ship.h"

#include <algorithm>
#include <cassert>
#include <random>

#include "champsim.h"
champsim::modules::replacement::register_module<ship> ship_register("ship");

// initialize replacement state. num_consumers is the number of consumers whose
// producers in the system — published to the globals builder by the env
// before module construction. Falls back to 1 for tests that build a
// CACHE / ship pair directly without an env.
ship::ship(champsim::modules::ModuleBuilder builder)
    : NUM_SET(builder.get_parent<champsim::modules::cache_module>()->num_sets()), NUM_WAY(builder.get_parent<champsim::modules::cache_module>()->num_ways()),
      num_consumers_(builder.get_parameter<std::size_t>("num_consumers", true, std::size_t{1})),
      sampler(champsim::msl::get_num_samples(NUM_SET) * num_consumers_ * static_cast<std::size_t>(NUM_WAY)),
      rrpv_values(static_cast<std::size_t>(NUM_SET * NUM_WAY), maxRRPV), set_categorizer(champsim::msl::get_sample_rate(NUM_SET)),
      sampler_tag_bits(builder.get_parent<champsim::modules::cache_module>()->get_offset_bits())
{
  std::generate_n(std::back_inserter(SHCT), num_consumers_, []() -> typename decltype(SHCT)::value_type { return {}; });
}

void ship::initialize_replacement() {}

int& ship::get_rrpv(long set, long way) { return rrpv_values.at(static_cast<std::size_t>(set * NUM_WAY + way)); }

// find replacement victim
long ship::find_victim(champsim::origin origin, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
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
void ship::update_replacement_state(champsim::origin origin, long set, long way, champsim::address full_addr, champsim::address ip,
                                    champsim::address victim_addr, access_type type, bool hit)
{
  using namespace champsim::data::data_literals;

  // update sampler
  if (set_categorizer.get_sample_category(set) == 0) {
    auto s_idx = set / champsim::msl::get_sample_rate(NUM_SET);
    auto s_set_begin = std::next(std::begin(sampler), s_idx * NUM_WAY + champsim::msl::get_num_samples(NUM_SET) * NUM_WAY * origin.cpu());
    auto s_set_end = std::next(s_set_begin, NUM_WAY);

    // check hit
    auto match = std::find_if(s_set_begin, s_set_end, [addr = full_addr, shamt = sampler_tag_bits](auto x) {
      return x.valid && x.address.slice_upper(shamt) == addr.slice_upper(shamt);
    });
    if (match != s_set_end) {
      auto SHCT_idx = match->ip.slice_lower<32_b>().to<std::size_t>() % SHCT_PRIME;
      SHCT[origin.cpu()][SHCT_idx] -= 1;

      match->used = true;
    } else {
      match = std::min_element(s_set_begin, s_set_end, [](auto x, auto y) { return x.last_used < y.last_used; });

      if (!match->used) {
        auto SHCT_idx = match->ip.slice_lower<32_b>().to<std::size_t>() % SHCT_PRIME;
        SHCT[origin.cpu()][SHCT_idx] += 1;
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
}

void ship::replacement_cache_fill(champsim::origin origin, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                  access_type type)
{
  // handle writeback access
  if (access_type{type} == access_type::WRITE) {
    get_rrpv(set, way) = maxRRPV - 1;
    return;
  }

  using namespace champsim::data::data_literals;
  // SHIP prediction
  auto SHCT_idx = ip.slice_lower<32_b>().to<std::size_t>() % SHCT_PRIME;

  get_rrpv(set, way) = maxRRPV - 1;
  if (SHCT[origin.cpu()][SHCT_idx].is_max())
    get_rrpv(set, way) = maxRRPV;
}
