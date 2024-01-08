#include "direct_predictor.h"

#include "instruction.h"

auto direct_predictor::check_hit(champsim::address ip) -> std::optional<btb_entry_t>
{
  return BTB.check_hit({ip, champsim::address{}, branch_info::ALWAYS_TAKEN});
}

void direct_predictor::update(champsim::address ip, champsim::address branch_target, uint8_t branch_type)
{
  // update btb entry
  auto type = branch_info::ALWAYS_TAKEN;
  if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL))
    type = branch_info::INDIRECT;
  else if (branch_type == BRANCH_RETURN)
    type = branch_info::RETURN;
  else if (branch_type == BRANCH_CONDITIONAL)
    type = branch_info::CONDITIONAL;

  auto opt_entry = BTB.check_hit({ip, branch_target, type});
  if (opt_entry.has_value()) {
    opt_entry->type = type;
    if (branch_target != champsim::address{})
      opt_entry->target = branch_target;
  }

  if (branch_target != champsim::address{}) {
    BTB.fill(opt_entry.value_or(btb_entry_t{ip, branch_target, type}));
  }
}
