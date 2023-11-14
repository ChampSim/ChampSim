#ifndef BTB_BASIC_BTB_RETURN_STACK_H
#define BTB_BASIC_BTB_RETURN_STACK_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <deque>

struct return_stack {
  static constexpr std::size_t max_size = 64;
  static constexpr std::size_t num_call_size_trackers = 1024;

  std::deque<uint64_t> stack;

  /*
   * The following structure identifies the size of call instructions so we can
   * find the target for a call's return, since calls may have different sizes.
   */
  std::array<uint64_t, num_call_size_trackers> call_size_trackers;

  return_stack() { std::fill(std::begin(call_size_trackers), std::end(call_size_trackers), 4); }

  std::pair<uint64_t, bool> prediction(uint64_t ip);
  void push(uint64_t ip);
  void calibrate_call_size(uint64_t branch_target);
};

#endif
