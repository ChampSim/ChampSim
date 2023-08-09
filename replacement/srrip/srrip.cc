#include <cassert>

#include "cache.h"
#include <unordered_map>

namespace
{
constexpr int maxRRPV = 3;
std::unordered_map<CACHE*, std::vector<int>> rrpv_values;
} // namespace

// initialize replacement state
void CACHE::initialize_replacement() { ::rrpv_values[this] = std::vector<int>(NUM_SET * NUM_WAY, ::maxRRPV); }

// find replacement victim
uint32_t CACHE::find_victim(uint32_t triggering_cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
  // look for the maxRRPV line
  auto begin = std::next(std::begin(::rrpv_values[this]), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);
  auto victim = std::find(begin, end, ::maxRRPV); // hijack the lru field
  while (victim == end) {
    for (auto it = begin; it != end; ++it)
      ++(*it);

    victim = std::find(begin, end, ::maxRRPV);
  }

  assert(begin <= victim);
  assert(victim < end);
  return static_cast<uint32_t>(std::distance(begin, victim)); // cast protected by assertions
}

// called on every cache hit and cache fill
void CACHE::update_replacement_state(uint32_t triggering_cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type,
                                     uint8_t hit)
{
  if (hit)
    ::rrpv_values[this][set * NUM_WAY + way] = 0;
  else
    ::rrpv_values[this][set * NUM_WAY + way] = ::maxRRPV - 1;
}

// use this function to print out your own stats at the end of simulation
void CACHE::replacement_final_stats() {}
