#ifndef BTB_BASIC_BTB_DIRECT_PREDICTOR_H
#define BTB_BASIC_BTB_DIRECT_PREDICTOR_H

#include <cstdint>
#include <optional>

#include "msl/lru_table.h"

struct direct_predictor
{
  enum class branch_info {
    INDIRECT,
    RETURN,
    ALWAYS_TAKEN,
    CONDITIONAL,
  };

  static constexpr std::size_t sets = 1024;
  static constexpr std::size_t ways = 8;

  struct btb_entry_t {
    uint64_t ip_tag = 0;
    uint64_t target = 0;
    branch_info type = branch_info::ALWAYS_TAKEN;

    auto index() const { return ip_tag >> 2; }
    auto tag() const { return ip_tag >> 2; }
  };

  champsim::msl::lru_table<btb_entry_t> BTB{sets, ways};
  std::optional<btb_entry_t> check_hit(uint64_t ip);
  void update(uint64_t ip, uint64_t branch_target, uint8_t branch_type);
};

#endif
