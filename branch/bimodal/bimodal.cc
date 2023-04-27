#include "bimodal.h"
#include <iostream>

void bimodal::initialize_branch_predictor() { std::cout << "CPU " << intern_->cpu << " Bimodal branch predictor" << std::endl; }

bool bimodal::predict_branch(champsim::address ip)
{
  auto value = bimodal_table[hash(ip)];
  return value.value() >= (value.maximum / 2);
}

void bimodal::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{
  bimodal_table[hash(ip)] += taken ? 1 : -1;
}
