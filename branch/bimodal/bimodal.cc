#include <map>

#include "ooo_cpu.h"

constexpr std::size_t BIMODAL_TABLE_SIZE = 16384;
constexpr std::size_t BIMODAL_PRIME = 16381;
constexpr std::size_t COUNTER_BITS = 2;

std::map<O3_CPU*, std::array<int, BIMODAL_TABLE_SIZE>> bimodal_table;

void O3_CPU::initialize_branch_predictor()
{
  std::cout << "CPU " << cpu << " Bimodal branch predictor" << std::endl;
  bimodal_table[this] = {};
}

uint8_t O3_CPU::predict_branch(uint64_t ip, uint64_t predicted_target, uint8_t always_taken, uint8_t branch_type)
{
  uint32_t hash = ip % BIMODAL_PRIME;

  return bimodal_table[this][hash] >= (1 << (COUNTER_BITS - 1));
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  uint32_t hash = ip % BIMODAL_PRIME;

  if (taken)
    bimodal_table[this][hash] = std::min(bimodal_table[this][hash] + 1, ((1 << COUNTER_BITS) - 1));
  else
    bimodal_table[this][hash] = std::max(bimodal_table[this][hash] - 1, 0);
}
