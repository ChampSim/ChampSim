#ifndef BTB_BASIC_BTB_INDIRECT_PREDICTOR_H
#define BTB_BASIC_BTB_INDIRECT_PREDICTOR_H

#include <array>
#include <bitset>
#include <cstdint>
#include <utility>

#include "address.h"
#include "champsim.h"
#include "msl/bits.h"

struct indirect_predictor {
  static constexpr std::size_t size = 4096;
  std::array<champsim::address, size> predictor = {};
  std::bitset<champsim::msl::lg2(size)> conditional_history = {};

  std::pair<champsim::address, bool> prediction(champsim::address ip);
  void update_target(champsim::address ip, champsim::address branch_target);
  void update_direction(bool taken);
};

#endif
