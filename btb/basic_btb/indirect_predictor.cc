#include "indirect_predictor.h"

std::pair<champsim::address, bool> indirect_predictor::prediction(champsim::address ip)
{
  auto hash = ip.slice_upper<2>().to<unsigned long long>() ^ conditional_history.to_ullong();
  return {predictor[hash % std::size(predictor)], true};
}

void indirect_predictor::update_target(champsim::address ip, champsim::address branch_target)
{
  auto hash = ip.slice_upper<2>().to<unsigned long long>() ^ conditional_history.to_ullong();
  predictor[hash % std::size(predictor)] = branch_target;
}

void indirect_predictor::update_direction(bool taken)
{
  conditional_history <<= 1;
  conditional_history.set(0, taken);
}
