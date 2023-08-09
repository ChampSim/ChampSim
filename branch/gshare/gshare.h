#ifndef BRANCH_GSHARE_H
#define BRANCH_GSHARE_H

#include <array>
#include <bitset>

#include "modules.h"
#include "msl/fwcounter.h"

struct gshare : champsim::modules::branch_predictor {
  static constexpr std::size_t GLOBAL_HISTORY_LENGTH = 14;
  static constexpr std::size_t COUNTER_BITS = 2;
  static constexpr std::size_t GS_HISTORY_TABLE_SIZE = 16384;

  std::bitset<GLOBAL_HISTORY_LENGTH> branch_history_vector;
  std::array<champsim::msl::fwcounter<COUNTER_BITS>, GS_HISTORY_TABLE_SIZE> gs_history_table;

  using branch_predictor::branch_predictor;

  std::size_t gs_table_hash(uint64_t ip, std::bitset<GLOBAL_HISTORY_LENGTH> bh_vector);
  bool predict_branch(uint64_t ip);
  void last_branch_result(uint64_t ip, uint64_t branch_target, bool taken, uint8_t branch_type);
};

#endif
