#ifndef BTB_BASIC_BTB_DIRECT_PREDICTOR_H
#define BTB_BASIC_BTB_DIRECT_PREDICTOR_H

#include <cstdint>
#include <optional>

#include "champsim.h"
#include "address.h"
#include "msl/lru_table.h"

struct direct_predictor {
  enum class branch_info {
    INDIRECT,
    RETURN,
    ALWAYS_TAKEN,
    CONDITIONAL,
  };

  static constexpr std::size_t sets = 1024;
  static constexpr std::size_t ways = 8;

  struct btb_entry_t {
    champsim::address ip_tag{};
    champsim::address target{};
    branch_info type = branch_info::ALWAYS_TAKEN;

    auto index() const { return ip_tag.slice_upper<2>(); }
    auto tag() const { return ip_tag.slice_upper<2>(); }
  };

  champsim::msl::lru_table<btb_entry_t> BTB{sets, ways};
  std::optional<btb_entry_t> check_hit(champsim::address ip);
  void update(champsim::address ip, champsim::address branch_target, uint8_t branch_type);
};

#endif
