#ifndef REPLACEMENT_RANDOM_H
#define REPLACEMENT_RANDOM_H

#include <random>

#include "cache.h"
#include "modules.h"

struct random : public champsim::modules::replacement {
  std::mt19937_64 rng{};
  std::uniform_int_distribution<long> dist;

  void initialize_replacement();
  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip, champsim::address full_addr,
                         access_type type);
  // void update_replacement_state(uint32_t triggering_cpu, long set, long way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, access_type type, uint8_t
  // hit);
  //  void replacement_final_stats()
};

#endif
