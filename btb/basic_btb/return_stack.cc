#include "return_stack.h"

std::pair<uint64_t, uint8_t> return_stack::prediction(uint64_t ip)
{
  if (std::empty(stack))
    return {0, true};

  // peek at the top of the RAS and adjust for the size of the call instr
  auto target = stack.back();
  auto size = call_size_trackers[target % std::size(call_size_trackers)];

  return {target + size, true};
}

void return_stack::push(uint64_t ip)
{
  stack.push_back(ip);
  if (std::size(stack) > max_size)
    stack.pop_front();
}

void return_stack::calibrate_call_size(uint64_t branch_target)
{
  if (!std::empty(stack)) {
    // recalibrate call-return offset if our return prediction got us close, but not exact
    auto call_ip = stack.back();
    stack.pop_back();

    auto estimated_call_instr_size = (call_ip > branch_target) ? call_ip - branch_target : branch_target - call_ip;
    if (estimated_call_instr_size <= 10) {
      call_size_trackers[call_ip % std::size(call_size_trackers)] = estimated_call_instr_size;
    }
  }
}

