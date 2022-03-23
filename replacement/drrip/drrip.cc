#include <algorithm>
#include <map>
#include <utility>

#include "cache.h"

#define maxRRPV 3
#define NUM_POLICY 2
#define SDM_SIZE 32
#define TOTAL_SDM_SETS NUM_CPUS* NUM_POLICY* SDM_SIZE
#define BIP_MAX 32
#define PSEL_WIDTH 10
#define PSEL_MAX ((1 << PSEL_WIDTH) - 1)
#define PSEL_THRS PSEL_MAX / 2

std::map<CACHE*, unsigned> bip_counter;
std::map<CACHE*, std::vector<std::size_t>> rand_sets;
std::map<std::pair<CACHE*, std::size_t>, unsigned> PSEL;

void CACHE::initialize_replacement()
{
  // randomly selected sampler sets
  std::size_t rand_seed = 1103515245 + 12345;
  for (std::size_t i = 0; i < TOTAL_SDM_SETS; i++) {
    std::size_t val = (rand_seed / 65536) % NUM_SET;
    auto loc = std::lower_bound(std::begin(rand_sets[this]), std::end(rand_sets[this]), val);

    while (loc != std::end(rand_sets[this]) && *loc == val) {
      rand_seed = rand_seed * 1103515245 + 12345;
      val = (rand_seed / 65536) % NUM_SET;
      loc = std::lower_bound(std::begin(rand_sets[this]), std::end(rand_sets[this]), val);
    }

    rand_sets[this].insert(loc, val);
  }
}

// called on every cache hit and cache fill
void CACHE::update_replacement_state(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type,
                                     uint8_t hit)
{
  // do not update replacement state for writebacks
  if (type == WRITEBACK) {
    block[set * NUM_WAY + way].lru = maxRRPV - 1;
    return;
  }

  // cache hit
  if (hit) {
    block[set * NUM_WAY + way].lru = 0; // for cache hit, DRRIP always promotes
                                        // a cache line to the MRU position
    return;
  }

  // cache miss
  auto begin = std::next(std::begin(rand_sets[this]), cpu * NUM_POLICY * SDM_SIZE);
  auto end = std::next(begin, NUM_POLICY * SDM_SIZE);
  auto leader = std::find(begin, end, set);

  if (leader == end) // follower sets
  {
    if (PSEL[std::make_pair(this, cpu)] > PSEL_THRS) // follow BIP
    {
      block[set * NUM_WAY + way].lru = maxRRPV;

      bip_counter[this]++;
      if (bip_counter[this] == BIP_MAX)
        bip_counter[this] = 0;
      if (bip_counter[this] == 0)
        block[set * NUM_WAY + way].lru = maxRRPV - 1;
    } else // follow SRRIP
    {
      block[set * NUM_WAY + way].lru = maxRRPV - 1;
    }
  } else if (leader == begin) // leader 0: BIP
  {
    if (PSEL[std::make_pair(this, cpu)] > 0)
      PSEL[std::make_pair(this, cpu)]--;
    block[set * NUM_WAY + way].lru = maxRRPV;

    bip_counter[this]++;
    if (bip_counter[this] == BIP_MAX)
      bip_counter[this] = 0;
    if (bip_counter[this] == 0)
      block[set * NUM_WAY + way].lru = maxRRPV - 1;
  } else if (leader == std::next(begin)) // leader 1: SRRIP
  {
    if (PSEL[std::make_pair(this, cpu)] < PSEL_MAX)
      PSEL[std::make_pair(this, cpu)]++;
    block[set * NUM_WAY + way].lru = maxRRPV - 1;
  }
}

// find replacement victim
uint32_t CACHE::find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK* current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
  // look for the maxRRPV line
  auto begin = std::next(std::begin(block), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);
  auto victim = std::find_if(begin, end, [](BLOCK x) { return x.lru == maxRRPV; }); // hijack the lru field
  while (victim == end) {
    for (auto it = begin; it != end; ++it)
      it->lru++;

    victim = std::find_if(begin, end, [](BLOCK x) { return x.lru == maxRRPV; });
  }

  return std::distance(begin, victim);
}

// use this function to print out your own stats at the end of simulation
void CACHE::replacement_final_stats() {}
