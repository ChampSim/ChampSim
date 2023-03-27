#include "bimodal.h"
#include <iostream>

void bimodal::initialize_branch_predictor() { std::cout << "CPU " << intern_->cpu << " Bimodal branch predictor" << std::endl; }

uint8_t bimodal::predict_branch(uint64_t ip)
{
  auto value = bimodal_table[hash(ip)];
  return value.value() >= (value.maximum / 2);
}

void bimodal::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  bimodal_table[hash(ip)] += taken ? 1 : -1;
}
