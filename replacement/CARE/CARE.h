#ifndef REPLACEMENT_CARE_H
#define REPLACEMENT_CARE_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "cache.h"
#include "modules.h"
#include "msl/fwcounter.h"

struct CARE : public champsim::modules::replacement {
private:
  enum class reuse_prediction { low, moderate, high };
  enum class cost_prediction { low, medium, high };

  struct block_state {
    uint8_t epv = 3;
    uint8_t pmcs = 0;
    bool referenced = false;
    bool prefetched = false;
    bool valid = false;
    uint16_t signature = 0;
    uint32_t owner_cpu = 0;
  };

  struct sht_entry {
    champsim::msl::fwcounter<3> rc{};
    champsim::msl::fwcounter<3> pd{};
  };

  static constexpr std::size_t signature_bits = 14;
  static constexpr std::size_t sht_entries = std::size_t{1} << signature_bits;
  static constexpr std::size_t dtrm_period = 16384;
  static constexpr std::size_t sample_divisor = 64;
  static constexpr uint8_t max_epv = 3;
  static constexpr uint8_t low_pmcs_state = 0;
  static constexpr uint8_t high_pmcs_state = 3;

  long num_sets;
  long num_ways;

  std::vector<block_state> blocks;
  std::vector<bool> sampled_sets;
  std::vector<std::array<sht_entry, sht_entries>> sht;

  double pmc_low = 50.0;
  double pmc_high = 350.0;
  std::size_t miss_counter = 0;
  std::size_t costly_counter = 0;

  [[nodiscard]] std::size_t block_index(long set, long way) const;
  [[nodiscard]] bool is_sampled(long set) const;
  [[nodiscard]] uint16_t make_signature(champsim::address ip, bool is_prefetch) const;
  [[nodiscard]] std::size_t signature_index(uint16_t signature) const;
  [[nodiscard]] sht_entry& entry_for(uint32_t cpu, uint16_t signature);
  [[nodiscard]] const sht_entry& entry_for(uint32_t cpu, uint16_t signature) const;
  [[nodiscard]] reuse_prediction predict_reuse(uint32_t cpu, uint16_t signature) const;
  [[nodiscard]] cost_prediction predict_cost(uint32_t cpu, uint16_t signature) const;
  [[nodiscard]] uint8_t select_epv(reuse_prediction reuse, cost_prediction cost, access_type type) const;
  [[nodiscard]] uint8_t quantize(double pmc, double low, double high) const;
  void update_dtrm(bool costly);
  void update_history_on_eviction(std::size_t block_idx);

public:
  explicit CARE(CACHE* cache);

  long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const champsim::cache_block* current_set,
                   champsim::address ip, champsim::address full_addr, access_type type);
  void update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr,
                                champsim::address ip, champsim::address victim_addr, access_type type, uint8_t hit);
  void replacement_cache_fill(uint32_t triggering_cpu, long set, long way, champsim::address full_addr,
                              champsim::address ip, champsim::address victim_addr, access_type type, double pmc);
};

#endif
