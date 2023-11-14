#ifndef REPLACEMENT_SRRIP_H
#define REPLACEMENT_SRRIP_H

#include <cstdint>
#include <vector>

#include "cache.h"
#include "modules.h"

struct srrip_set_helper {
  using rrpv_type = int;
  static constexpr rrpv_type maxRRPV = 3;

  std::vector<rrpv_type> rrpv_values;
  rrpv_type& get_rrpv(long way);

  explicit srrip_set_helper(long ways);

  long victim();
  void update(long way, bool hit);
};

struct srrip : champsim::modules::replacement {

  std::vector<srrip_set_helper> sets;

  explicit srrip(CACHE* cache);
  srrip(CACHE* cache, long sets_, long ways_);

  // void initialize_replacement() {}
  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const CACHE::BLOCK* current_set, uint64_t ip, uint64_t full_addr, access_type type);
  void update_replacement_state(uint32_t triggering_cpu, long set, long way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, access_type type,
                                uint8_t hit);

  // use this function to print out your own stats at the end of simulation
  // void replacement_final_stats() {}
};

#endif
