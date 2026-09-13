#ifndef REPLACEMENT_SHIP_H
#define REPLACEMENT_SHIP_H

#include <array>
#include <vector>

#include "cache.h"
#include "modules.h"
#include "msl/bits.h"
#include "msl/stat_methods.h"

struct ship : public champsim::modules::replacement {
private:
  int& get_rrpv(long set, long way);

public:
  static constexpr int maxRRPV = 3;
  static constexpr std::size_t SHCT_SIZE = 16384;
  static constexpr unsigned SHCT_PRIME = 16381;
  static constexpr unsigned SHCT_MAX = 7;

  // sampler structure
  class SAMPLER_class
  {
  public:
    bool valid = false;
    bool used = false;
    champsim::address address{};
    champsim::address ip{};
    uint64_t last_used = 0;
  };

  long NUM_SET, NUM_WAY;
  std::size_t num_consumers_;
  uint64_t access_count = 0;

  // sampler
  std::vector<SAMPLER_class> sampler;
  std::vector<int> rrpv_values;
  champsim::msl::categorizer<long> set_categorizer;
  champsim::data::bits sampler_tag_bits;

  // prediction table structure
  std::vector<std::array<champsim::msl::fwcounter<champsim::msl::lg2(SHCT_MAX + 1)>, SHCT_SIZE>> SHCT;

  explicit ship(champsim::modules::ModuleBuilder builder);

  void initialize_replacement() override;
  long find_victim(champsim::origin origin, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type) override;
  void replacement_cache_fill(champsim::origin origin, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                              access_type type) override;
  void update_replacement_state(champsim::origin origin, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                access_type type, bool hit) override;

  void replacement_final_stats() override {}
};

#endif
