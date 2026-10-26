#ifndef REPLACEMENT_BRRIP_H
#define REPLACEMENT_BRRIP_H

#include <cstdint>
#include <vector>

#include "cache.h"
#include "modules.h"

struct brrip_set_helper {
  using rrpv_type = int;
  static constexpr rrpv_type maxRRPV = 3;

  std::vector<rrpv_type> rrpv_values;
  rrpv_type& get_rrpv(long way);

  explicit brrip_set_helper(long ways);

  long victim();
  void update(long way, bool hit, rrpv_type insertion_rrpv);
};

struct brrip : public champsim::modules::replacement {
  static constexpr std::uint32_t BRRIP_MAX = 32;
  std::uint32_t fill_counter{0};
  std::uint64_t fills{};
  std::uint64_t eligible_fills{};
  std::uint64_t writeback_fills{};
  std::uint64_t rrpv3_insertions{};
  std::uint64_t rrpv2_insertions{};
  std::uint64_t writeback_rrpv2_insertions{};
  std::vector<brrip_set_helper> sets;

  explicit brrip(CACHE* cache);
  brrip(CACHE* cache, long sets_, long ways_);

  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type);
  void update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                champsim::address victim_addr, access_type type, uint8_t hit);
  void replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                              champsim::address victim_addr, access_type type);
  void replacement_final_stats();
};

#endif

