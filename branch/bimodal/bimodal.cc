#include <map>

#include "msl/fwcounter.h"
#include "ooo_cpu.h"

namespace
{
constexpr std::size_t BIMODAL_TABLE_SIZE = 16384;
constexpr std::size_t BIMODAL_PRIME = 16381;
constexpr std::size_t COUNTER_BITS = 2;

std::map<O3_CPU*, std::array<champsim::msl::fwcounter<COUNTER_BITS>, BIMODAL_TABLE_SIZE>> bimodal_table;
} // namespace

void O3_CPU::initialize_branch_predictor() { std::cout << "CPU " << cpu << " Bimodal branch predictor" << std::endl; }

uint8_t O3_CPU::predict_branch(uint64_t ip)
{
  auto hash = ip % ::BIMODAL_PRIME;
  auto value = ::bimodal_table[this][hash];

  return value.value() >= (value.maximum / 2);
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  auto hash = ip % ::BIMODAL_PRIME;
  ::bimodal_table[this][hash] += taken ? 1 : -1;
}
