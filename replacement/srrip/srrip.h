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

struct srrip : public champsim::modules::replacement {

  std::vector<srrip_set_helper> sets;

  explicit srrip(champsim::modules::ModuleBuilder builder);
  srrip(champsim::modules::cache_module* cache, long sets_, long ways_);

  void initialize_replacement() override {}
  long find_victim(champsim::origin origin, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type) override;
  void update_replacement_state(champsim::origin origin, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                access_type type, bool hit) override;
  void replacement_cache_fill(champsim::origin /*origin*/, long /*set*/, long /*way*/, champsim::address /*full_addr*/, champsim::address /*ip*/,
                              champsim::address /*victim_addr*/, access_type /*type*/) override
  {
  }
  void replacement_final_stats() override {}
};

#endif
