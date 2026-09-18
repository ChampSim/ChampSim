#ifndef REPLACEMENT_RANDOM_H
#define REPLACEMENT_RANDOM_H

#include <random>

#include "cache.h"
#include "modules.h"
#include "util/random.h"

struct random : public champsim::modules::replacement {
  std::mt19937_64 rng{};
  champsim::uniform_int_distribution<long> dist;

  explicit random(champsim::modules::ModuleBuilder builder);
  random(champsim::modules::cache_module* cache, long ways);

  void initialize_replacement() override {}
  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type) override;
  void update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                access_type type, bool hit) override
  {
  }
  void replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                              access_type type) override
  {
  }
  void replacement_final_stats() override {}
};

#endif
