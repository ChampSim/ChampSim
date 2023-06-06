#include "bimodal.h"

bool bimodal::predict_branch(uint64_t ip)
{
  auto value = bimodal_table[hash(ip)];
  return value.value() >= (value.maximum / 2);
}

void bimodal::last_branch_result(uint64_t ip, uint64_t branch_target, bool taken, uint8_t branch_type)
{
  bimodal_table[hash(ip)] += taken ? 1 : -1;
}
