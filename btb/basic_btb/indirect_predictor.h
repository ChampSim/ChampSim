#ifndef BTB_BASIC_BTB_INDIRECT_PREDICTOR_H
#define BTB_BASIC_BTB_INDIRECT_PREDICTOR_H

#include <array>
#include <bitset>
#include <cstdint>
#include <utility>

#include "msl/bits.h"

struct indirect_predictor {
  static constexpr std::size_t size = 4096;
  std::array<uint64_t, size> predictor = {};
  std::bitset<champsim::msl::lg2(size)> conditional_history = {};

  std::pair<uint64_t, bool> prediction(uint64_t ip);
  void update_target(uint64_t ip, uint64_t branch_target);
  void update_direction(bool taken);
};

#endif
