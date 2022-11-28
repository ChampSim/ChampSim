#include <algorithm>
#include <map>
#include <utility>

#include "cache.h"
#include "msl/fwcounter.h"

namespace
{
constexpr unsigned maxRRPV = 3;
constexpr std::size_t NUM_POLICY = 2;
constexpr std::size_t SDM_SIZE = 32;
constexpr std::size_t TOTAL_SDM_SETS = NUM_CPUS * NUM_POLICY * SDM_SIZE;
constexpr unsigned BIP_MAX = 32;
constexpr unsigned PSEL_WIDTH = 10;

std::map<CACHE*, unsigned> bip_counter;
std::map<CACHE*, std::vector<std::size_t>> rand_sets;
std::map<std::pair<CACHE*, std::size_t>, champsim::msl::fwcounter<PSEL_WIDTH>> PSEL;
std::map<CACHE*, std::vector<unsigned>> rrpv;
} // namespace

void CACHE::initialize_replacement()
{
  // randomly selected sampler sets
  std::size_t rand_seed = 1103515245 + 12345;
  for (std::size_t i = 0; i < ::TOTAL_SDM_SETS; i++) {
    std::size_t val = (rand_seed / 65536) % NUM_SET;
    auto loc = std::lower_bound(std::begin(::rand_sets[this]), std::end(::rand_sets[this]), val);

    while (loc != std::end(::rand_sets[this]) && *loc == val) {
      rand_seed = rand_seed * 1103515245 + 12345;
      val = (rand_seed / 65536) % NUM_SET;
      loc = std::lower_bound(std::begin(::rand_sets[this]), std::end(::rand_sets[this]), val);
    }

    ::rand_sets[this].insert(loc, val);
  }

  ::rrpv.insert({this, std::vector<unsigned>(NUM_SET * NUM_WAY)});
}

// called on every cache hit and cache fill
void CACHE::update_replacement_state(uint32_t triggering_cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type,
                                     uint8_t hit)
{
  // do not update replacement state for writebacks
  if (type == WRITE) {
    ::rrpv[this][set * NUM_WAY + way] = ::maxRRPV - 1;
    return;
  }

  // cache hit
  if (hit) {
    ::rrpv[this][set * NUM_WAY + way] = 0; // for cache hit, DRRIP always promotes a cache line to the MRU position
    return;
  }

  // cache miss
  auto begin = std::next(std::begin(::rand_sets[this]), triggering_cpu * ::NUM_POLICY * ::SDM_SIZE);
  auto end = std::next(begin, ::NUM_POLICY * ::SDM_SIZE);
  auto leader = std::find(begin, end, set);

  if (leader == end) { // follower sets
    auto selector = ::PSEL[std::make_pair(this, triggering_cpu)];
    if (selector.value() > (selector.maximum / 2)) { // follow BIP
      ::rrpv[this][set * NUM_WAY + way] = ::maxRRPV;

      ::bip_counter[this]++;
      if (::bip_counter[this] == ::BIP_MAX) {
        ::bip_counter[this] = 0;
        ::rrpv[this][set * NUM_WAY + way] = ::maxRRPV - 1;
      }
    } else { // follow SRRIP
      ::rrpv[this][set * NUM_WAY + way] = ::maxRRPV - 1;
    }
  } else if (leader == begin) { // leader 0: BIP
    ::PSEL[std::make_pair(this, triggering_cpu)]--;
    ::rrpv[this][set * NUM_WAY + way] = ::maxRRPV;

    ::bip_counter[this]++;
    if (::bip_counter[this] == ::BIP_MAX) {
      ::bip_counter[this] = 0;
      ::rrpv[this][set * NUM_WAY + way] = ::maxRRPV - 1;
    }
  } else if (leader == std::next(begin)) { // leader 1: SRRIP
    ::PSEL[std::make_pair(this, triggering_cpu)]++;
    ::rrpv[this][set * NUM_WAY + way] = ::maxRRPV - 1;
  }
}

// find replacement victim
uint32_t CACHE::find_victim(uint32_t triggering_cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
  // look for the maxRRPV line
  auto begin = std::next(std::begin(::rrpv[this]), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);

  auto victim = std::max_element(begin, end);
  for (auto it = begin; it != end; ++it)
    *it += ::maxRRPV - *victim;

  assert(begin <= victim);
  assert(victim < end);
  return static_cast<uint32_t>(std::distance(begin, victim)); // cast protected by assertions
}

// use this function to print out your own stats at the end of simulation
void CACHE::replacement_final_stats() {}
