#include "indirect_predictor.h"

std::pair<uint64_t, bool> indirect_predictor::prediction(uint64_t ip)
{
  auto hash = (ip >> 2) ^ conditional_history.to_ullong();
  return {predictor[hash % std::size(predictor)], true};
}

void indirect_predictor::update_target(uint64_t ip, uint64_t branch_target)
{
  auto hash = (ip >> 2) ^ conditional_history.to_ullong();
  predictor[hash % std::size(predictor)] = branch_target;
}

void indirect_predictor::update_direction(bool taken)
{
  conditional_history <<= 1;
  conditional_history.set(0, taken);
}

