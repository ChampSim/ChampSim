#include <algorithm>
#include <array>
#include <map>
#include <utility>
#include <vector>

#include "cache.h"

#define maxRRPV 3
#define SHCT_SIZE 16384
#define SHCT_PRIME 16381
#define SAMPLER_SET (256 * NUM_CPUS)
#define SHCT_MAX 7

// sampler structure
class SAMPLER_class
{
public:
  bool valid = false;
  uint8_t type = 0, used = 0;
  uint64_t address = 0, cl_addr = 0, ip = 0;
  uint32_t lru = 9999999;
};

// sampler
std::map<CACHE*, std::vector<std::size_t>> rand_sets;
std::map<CACHE*, std::vector<SAMPLER_class>> sampler;

// prediction table structure
std::map<std::pair<CACHE*, std::size_t>, std::array<unsigned, SHCT_SIZE>> SHCT;

// initialize replacement state
void CACHE::initialize_replacement()
{
  // randomly selected sampler sets
  std::size_t rand_seed = 1103515245 + 12345;
  ;
  for (std::size_t i = 0; i < SAMPLER_SET; i++) {
    std::size_t val = (rand_seed / 65536) % NUM_SET;
    std::vector<std::size_t>::iterator loc = std::lower_bound(std::begin(rand_sets[this]), std::end(rand_sets[this]), val);

    while (loc != std::end(rand_sets[this]) && *loc == val) {
      rand_seed = rand_seed * 1103515245 + 12345;
      val = (rand_seed / 65536) % NUM_SET;
      loc = std::lower_bound(std::begin(rand_sets[this]), std::end(rand_sets[this]), val);
    }

    rand_sets[this].insert(loc, val);
  }

  sampler.emplace(this, SAMPLER_SET * NUM_WAY);
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

// called on every cache hit and cache fill
void CACHE::update_replacement_state(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type,
                                     uint8_t hit)
{
  // handle writeback access
  if (type == WRITEBACK) {
    if (!hit)
      block[set * NUM_WAY + way].lru = maxRRPV - 1;

    return;
  }

  // update sampler
  auto s_idx = std::find(std::begin(rand_sets[this]), std::end(rand_sets[this]), set);
  if (s_idx != std::end(rand_sets[this])) {
    auto s_set_begin = std::next(std::begin(sampler[this]), std::distance(std::begin(rand_sets[this]), s_idx));
    auto s_set_end = std::next(s_set_begin, NUM_WAY);

    // check hit
    auto match = std::find_if(s_set_begin, s_set_end, eq_addr<SAMPLER_class>(full_addr, 8 + lg2(NUM_WAY)));
    if (match != s_set_end) {
      uint32_t SHCT_idx = match->ip % SHCT_PRIME;
      if (SHCT[std::make_pair(this, cpu)][SHCT_idx] > 0)
        SHCT[std::make_pair(this, cpu)][SHCT_idx]--;

      match->type = type;
      match->used = 1;
    } else {
      match = std::max_element(s_set_begin, s_set_end, lru_comparator<SAMPLER_class, SAMPLER_class>());

      if (match->used) {
        uint32_t SHCT_idx = match->ip % SHCT_PRIME;
        if (SHCT[std::make_pair(this, cpu)][SHCT_idx] < SHCT_MAX)
          SHCT[std::make_pair(this, cpu)][SHCT_idx]++;
      }

      match->valid = 1;
      match->address = full_addr;
      match->ip = ip;
      match->type = type;
      match->used = 0;
    }

    // update LRU state
    for (auto it = s_set_begin; it != s_set_end; ++it)
      it->lru++;
    match->lru = 0;
  }

  if (hit)
    block[set * NUM_WAY + way].lru = 0;
  else {
    // SHIP prediction
    uint32_t SHCT_idx = ip % SHCT_PRIME;

    block[set * NUM_WAY + way].lru = maxRRPV - 1;
    if (SHCT[std::make_pair(this, cpu)][SHCT_idx] == SHCT_MAX)
      block[set * NUM_WAY + way].lru = maxRRPV;
  }
}

// use this function to print out your own stats at the end of simulation
void CACHE::replacement_final_stats() {}
