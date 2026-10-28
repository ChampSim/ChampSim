#ifndef REPLACEMENT_LRU_H
#define REPLACEMENT_LRU_H

#include <vector>

#include "cache.h"
#include "modules.h"

class lru : public champsim::modules::replacement
{
  long NUM_WAY;
  std::vector<uint64_t> last_used_cycles;
  uint64_t cycle = 0;

public:
  explicit lru(champsim::modules::ModuleBuilder builder);
  lru(champsim::modules::cache_module* cache, long sets, long ways);

  void initialize_replacement() override {}
  long find_victim(champsim::origin origin, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type) override;
  void replacement_cache_fill(champsim::origin origin, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                              access_type type) override;
  void update_replacement_state(champsim::origin origin, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                access_type type, bool hit) override;
  void replacement_final_stats() override {}
};

#endif
