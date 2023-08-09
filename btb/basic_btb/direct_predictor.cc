#include "direct_predictor.h"

#include "instruction.h"

auto direct_predictor::check_hit(uint64_t ip) -> std::optional<btb_entry_t> { return BTB.check_hit({ip, 0, branch_info::ALWAYS_TAKEN}); }

void direct_predictor::update(uint64_t ip, uint64_t branch_target, uint8_t branch_type)
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
    if (branch_target != 0)
      opt_entry->target = branch_target;
  }

  if (branch_target != 0) {
    BTB.fill(opt_entry.value_or(btb_entry_t{ip, branch_target, type}));
  }
}
