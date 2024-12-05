#include "return_stack.h"

std::pair<champsim::address, bool> return_stack::prediction()
{
  if (std::empty(stack))
    return {champsim::address{}, true};

  // peek at the top of the RAS and adjust for the size of the call instr
  auto target = stack.back();
  auto size = call_size_trackers[target.slice_lower<champsim::data::bits{champsim::msl::lg2(num_call_size_trackers)}>().to<std::size_t>()];

  return {target + size, true};
}

void return_stack::push(champsim::address ip)
{
  stack.push_back(ip);
  if (std::size(stack) > max_size)
    stack.pop_front();
}

void return_stack::calibrate_call_size(champsim::address branch_target)
{
  if (!std::empty(stack)) {
    // recalibrate call-return offset if our return prediction got us close, but not exact
    auto call_ip = stack.back();
    stack.pop_back();

    static int num_times_returned_backwards = 0;
    if (call_ip > branch_target && num_times_returned_backwards < 10) {
      ++num_times_returned_backwards;
      fmt::print("[BTB] WARNING: target of return is a lower address than the corresponding call. This is usually a problem with your trace.\n");
    }

    auto estimated_call_instr_size = call_ip > branch_target ? champsim::uoffset(branch_target, call_ip) : champsim::uoffset(call_ip, branch_target);
    if (estimated_call_instr_size <= 10) {
      call_size_trackers[call_ip.slice_lower<champsim::data::bits{champsim::msl::lg2(num_call_size_trackers)}>().to<std::size_t>()] = estimated_call_instr_size;
    }
  }
}
