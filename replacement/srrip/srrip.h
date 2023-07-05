#ifndef REPLACEMENT_SRRIP_H
#define REPLACEMENT_SRRIP_H

#include <cstdint>
#include <vector>

#include "modules.h"
#include "cache.h"

struct srrip : champsim::modules::replacement
{
  static constexpr int maxRRPV = 3;

  std::size_t NUM_SET, NUM_WAY;
  std::vector<int> rrpv_values;

  srrip(CACHE* cache);

  //void initialize_replacement() {}
  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const CACHE::BLOCK* current_set, uint64_t ip, uint64_t full_addr, access_type type);
  void update_replacement_state(uint32_t triggering_cpu, long set, long way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, access_type type, uint8_t hit);

  // use this function to print out your own stats at the end of simulation
  //void replacement_final_stats() {}
};

#endif
