#ifndef REPLACEMENT_DRRIP_H
#define REPLACEMENT_DRRIP_H

#include <array>
#include <vector>

#include "cache.h"
#include "modules.h"
#include "msl/stat_methods.h"

struct drrip : public champsim::modules::replacement {
private:
  unsigned& get_rrpv(long set, long way);

public:
  static constexpr unsigned maxRRPV = 3;
  static constexpr unsigned BRRIP_MAX = 32;
  static constexpr unsigned PSEL_WIDTH = 10;

  enum class set_type { follower, brrip_leader, srrip_leader };

  long NUM_SET, NUM_WAY;

  unsigned brrip_counter = 0;

  std::vector<unsigned> rrpv;
  std::vector<champsim::msl::dscounter<long, PSEL_WIDTH>> PSEL;

  drrip(champsim::modules::ModuleBuilder builder);

  void initialize_replacement() override;
  long find_victim(champsim::origin origin, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type) override;
  void replacement_cache_fill(champsim::origin origin, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                              access_type type) override;
  void update_replacement_state(champsim::origin origin, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                access_type type, bool hit) override;

  void replacement_final_stats() override {}

  void update_brrip(long set, long way);
  void update_srrip(long set, long way);
};

#endif
