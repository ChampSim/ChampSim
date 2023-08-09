#ifndef REPLACEMENT_LRU_H
#define REPLACEMENT_LRU_H

#include <vector>

#include "cache.h"
#include "modules.h"

struct lru : champsim::modules::replacement {
  long NUM_SET, NUM_WAY;
  std::vector<uint64_t> last_used_cycles;
  uint64_t cycle = 0;

  lru(CACHE* cache);

  // void initialize_replacement();
  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const CACHE::BLOCK* current_set, uint64_t ip, uint64_t full_addr, access_type type);
  void update_replacement_state(uint32_t triggering_cpu, long set, long way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, access_type type,
                                uint8_t hit);
  // void replacement_final_stats()
};

#endif
