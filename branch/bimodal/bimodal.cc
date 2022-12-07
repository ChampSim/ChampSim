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

bool O3_CPU::predict_branch(champsim::address ip)
{
  auto hash = ip.slice_lower(champsim::lg2(BIMODAL_TABLE_SIZE)).to<uint64_t>() % ::BIMODAL_PRIME;
  auto value = ::bimodal_table[this][hash];

  return value.value() >= (value.maximum / 2);
}

void O3_CPU::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{
  auto hash = ip.slice_lower(champsim::lg2(BIMODAL_TABLE_SIZE)).to<uint64_t>() % ::BIMODAL_PRIME;
  ::bimodal_table[this][hash] += taken ? 1 : -1;
}
