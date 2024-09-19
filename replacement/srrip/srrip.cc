#include <cassert>

#include "cache.h"
#include <unordered_map>

namespace
{
constexpr int maxRRPV = 3;
std::unordered_map<CACHE*, std::vector<int>> rrpv;
} // namespace

// initialize replacement state
void CACHE::initialize_replacement() { ::rrpv[this] = std::vector<int>(NUM_SET * NUM_WAY, ::maxRRPV); }

// find replacement victim
uint32_t CACHE::find_victim(uint32_t triggering_cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
  // look for the maxRRPV line
  auto begin = std::next(std::begin(::rrpv[this]), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);

  auto victim = std::max_element(begin, end);
  auto rrpv_update = ::maxRRPV - *victim;
  if (rrpv_update != 0)
    for (auto it = begin; it != end; ++it)
      *it += rrpv_update;

  assert(begin <= victim);
  assert(victim < end);
  return static_cast<uint32_t>(std::distance(begin, victim)); // cast protected by assertions
}

// called on every cache hit and cache fill
void CACHE::update_replacement_state(uint32_t triggering_cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type,
                                     uint8_t hit)
{
  if (hit)
    ::rrpv[this][set * NUM_WAY + way] = 0;
  else
    ::rrpv[this][set * NUM_WAY + way] = ::maxRRPV - 1;
}

// use this function to print out your own stats at the end of simulation
void CACHE::replacement_final_stats() {}
