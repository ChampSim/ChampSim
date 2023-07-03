#ifndef REPLACEMENT_LRU_H
#define REPLACEMENT_LRU_H

#include <vector>

#include "cache.h"
#include "modules.h"

struct lru : champsim::modules::replacement
{
  std::size_t NUM_SET, NUM_WAY;
  std::vector<uint64_t> last_used_cycles;
  uint64_t cycle = 0;

  lru(CACHE* cache) : replacement(cache), NUM_SET(cache->NUM_SET), NUM_WAY(cache->NUM_WAY), last_used_cycles(NUM_SET*NUM_WAY, 0) {}

  //void initialize_replacement();
  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const CACHE::BLOCK* current_set, champsim::address ip, champsim::address full_addr, access_type type);
  void update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr, access_type type, uint8_t hit);
  //void replacement_final_stats()
};

#endif
