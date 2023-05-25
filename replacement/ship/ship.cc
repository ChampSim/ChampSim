#include <algorithm>
#include <array>
#include <cassert>
#include <map>
#include <utility>
#include <vector>

#include "cache.h"
#include "msl/bits.h"

namespace
{
constexpr int maxRRPV = 3;
constexpr std::size_t SHCT_SIZE = 16384;
constexpr unsigned SHCT_PRIME = 16381;
constexpr std::size_t SAMPLER_SET = (256 * NUM_CPUS);
constexpr unsigned SHCT_MAX = 7;

// sampler structure
class SAMPLER_class
{
public:
  bool valid = false;
  uint8_t used = 0;
  uint64_t address = 0, cl_addr = 0, ip = 0;
  uint64_t last_used = 0;
};

// sampler
std::map<CACHE*, std::vector<std::size_t>> rand_sets;
std::map<CACHE*, std::vector<SAMPLER_class>> sampler;
std::map<CACHE*, std::vector<int>> rrpv_values;

// prediction table structure
std::map<std::pair<CACHE*, std::size_t>, std::array<unsigned, SHCT_SIZE>> SHCT;
} // namespace

// initialize replacement state
void CACHE::initialize_replacement()
{
  // randomly selected sampler sets
  std::size_t rand_seed = 1103515245 + 12345;
  ;
  for (std::size_t i = 0; i < ::SAMPLER_SET; i++) {
    std::size_t val = (rand_seed / 65536) % NUM_SET;
    std::vector<std::size_t>::iterator loc = std::lower_bound(std::begin(::rand_sets[this]), std::end(::rand_sets[this]), val);

    while (loc != std::end(::rand_sets[this]) && *loc == val) {
      rand_seed = rand_seed * 1103515245 + 12345;
      val = (rand_seed / 65536) % NUM_SET;
      loc = std::lower_bound(std::begin(::rand_sets[this]), std::end(::rand_sets[this]), val);
    }

    ::rand_sets[this].insert(loc, val);
  }

  sampler.emplace(this, ::SAMPLER_SET * NUM_WAY);

  ::rrpv_values[this] = std::vector<int>(NUM_SET * NUM_WAY, ::maxRRPV);
}

// find replacement victim
uint32_t CACHE::find_victim(uint32_t triggering_cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
  // look for the maxRRPV line
  auto begin = std::next(std::begin(::rrpv_values[this]), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);
  auto victim = std::find(begin, end, ::maxRRPV);
  while (victim == end) {
    for (auto it = begin; it != end; ++it)
      ++(*it);

    victim = std::find(begin, end, ::maxRRPV);
  }

  assert(begin <= victim);
  return static_cast<uint32_t>(std::distance(begin, victim)); // cast pretected by prior assert
}

// called on every cache hit and cache fill
void CACHE::update_replacement_state(uint32_t triggering_cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type,
                                     uint8_t hit)
{
  // handle writeback access
  if (access_type{type} == access_type::WRITE) {
    if (!hit)
      ::rrpv_values[this][set * NUM_WAY + way] = ::maxRRPV - 1;

    return;
  }

  // update sampler
  auto s_idx = std::find(std::begin(::rand_sets[this]), std::end(::rand_sets[this]), set);
  if (s_idx != std::end(::rand_sets[this])) {
    auto s_set_begin = std::next(std::begin(sampler[this]), std::distance(std::begin(::rand_sets[this]), s_idx));
    auto s_set_end = std::next(s_set_begin, NUM_WAY);

    // check hit
    auto match = std::find_if(s_set_begin, s_set_end,
                              [addr = full_addr, shamt = 8 + champsim::lg2(NUM_WAY)](auto x) { return x.valid && (x.address >> shamt) == (addr >> shamt); });
    if (match != s_set_end) {
      auto SHCT_idx = match->ip % ::SHCT_PRIME;
      if (::SHCT[std::make_pair(this, triggering_cpu)][SHCT_idx] > 0)
        ::SHCT[std::make_pair(this, triggering_cpu)][SHCT_idx]--;

      match->used = 1;
    } else {
      match = std::min_element(s_set_begin, s_set_end, [](auto x, auto y) { return x.last_used < y.last_used; });

      if (match->used) {
        auto SHCT_idx = match->ip % ::SHCT_PRIME;
        if (::SHCT[std::make_pair(this, triggering_cpu)][SHCT_idx] < ::SHCT_MAX)
          ::SHCT[std::make_pair(this, triggering_cpu)][SHCT_idx]++;
      }

      match->valid = 1;
      match->address = full_addr;
      match->ip = ip;
      match->used = 0;
    }

    // update LRU state
    match->last_used = current_cycle;
  }

  if (hit)
    ::rrpv_values[this][set * NUM_WAY + way] = 0;
  else {
    // SHIP prediction
    auto SHCT_idx = ip % ::SHCT_PRIME;

    ::rrpv_values[this][set * NUM_WAY + way] = ::maxRRPV - 1;
    if (::SHCT[std::make_pair(this, triggering_cpu)][SHCT_idx] == ::SHCT_MAX)
      ::rrpv_values[this][set * NUM_WAY + way] = ::maxRRPV;
  }
}

// use this function to print out your own stats at the end of simulation
void CACHE::replacement_final_stats() {}
