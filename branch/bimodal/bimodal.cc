#include "bimodal.h"

champsim::modules::branch_predictor::register_module<bimodal> bimodal_register("bimodal");

bool bimodal::predict_branch(champsim::address ip, champsim::address predicted_target, bool always_taken, uint8_t branch_type)
{
  auto value = bimodal_table[hash(ip)];
  return value.value() > (value.maximum / 2);
}

void bimodal::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{
  bimodal_table[hash(ip)] += taken ? 1 : -1;
}
