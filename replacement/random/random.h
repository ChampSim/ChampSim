#ifndef REPLACEMENT_RANDOM_H
#define REPLACEMENT_RANDOM_H

#include <random>

#include "cache.h"
#include "modules.h"

struct random : public champsim::modules::replacement {
  std::mt19937_64 rng{};
  std::uniform_int_distribution<long> dist;

  explicit random(champsim::modules::ModuleBuilder builder);
  random(champsim::modules::cache_module* cache, long ways);

  void initialize_replacement() override {}
  long find_victim(champsim::origin origin, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type) override;
  void update_replacement_state(champsim::origin origin, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                access_type type, bool hit) override
  {
  }
  void replacement_cache_fill(champsim::origin origin, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                              access_type type) override
  {
  }
  void replacement_final_stats() override {}
};

#endif
