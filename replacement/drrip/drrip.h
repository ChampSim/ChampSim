#ifndef REPLACEMENT_DRRIP_H
#define REPLACEMENT_DRRIP_H

#include <array>
#include <vector>

#include "cache.h"
#include "modules.h"
#include "msl/fwcounter.h"

struct drrip : public champsim::modules::replacement {
private:
  unsigned& get_rrpv(long set, long way);

public:
  static constexpr unsigned maxRRPV = 3;
  static constexpr unsigned BRRIP_MAX = 32;
  static constexpr unsigned PSEL_WIDTH = 10;

  enum class set_type {
    follower, brrip_leader, srrip_leader
  };

  long NUM_SET, NUM_WAY, SET_SAMPLE_RATE;

  unsigned brrip_counter;
  std::vector<champsim::msl::fwcounter<PSEL_WIDTH>> PSEL;
  std::vector<unsigned> rrpv;

  drrip(CACHE* cache);

  // void initialize_replacement()
  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set, champsim::address ip,
                   champsim::address full_addr, access_type type);
  void replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                              access_type type);
  void update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                access_type type, uint8_t hit);

  // use this function to print out your own stats at the end of simulation
  // void replacement_final_stats() {}

  void update_brrip(long set, long way);
  void update_srrip(long set, long way);

  [[nodiscard]] constexpr set_type get_set_type(long set) {
    auto mask = SET_SAMPLE_RATE - 1;
    auto shift = champsim::lg2(SET_SAMPLE_RATE);
    auto low_slice = set & mask;
    auto high_slice = (set >> shift) & mask;
    if (high_slice == ~low_slice) {
      return set_type::brrip_leader;
    } else if (high_slice == low_slice) {
      return set_type::srrip_leader;
    }
    return set_type::follower;
  }

};

#endif
