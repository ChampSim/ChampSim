#ifndef BTB_BASIC_BTB_RETURN_STACK_H
#define BTB_BASIC_BTB_RETURN_STACK_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>

#include "address.h"
#include "champsim.h"

struct return_stack {
  static constexpr std::size_t max_size = 64;
  static constexpr std::size_t num_call_size_trackers = 1024;

  std::deque<champsim::address> stack;

  /*
   * The following structure identifies the size of call instructions so we can
   * find the target for a call's return, since calls may have different sizes.
   */
  std::array<typename champsim::address::difference_type, num_call_size_trackers> call_size_trackers;

  return_stack() { std::fill(std::begin(call_size_trackers), std::end(call_size_trackers), 4); }

  std::pair<champsim::address, bool> prediction();
  void push(champsim::address ip);
  void calibrate_call_size(champsim::address branch_target);
};

#endif
