#include <map>

#include "msl/fwcounter.h"
#include "ooo_cpu.h"

namespace
{
constexpr std::size_t BIMODAL_TABLE_SIZE = 16384;
constexpr std::size_t COUNTER_BITS = 4;

std::map<O3_CPU*, std::array<champsim::msl::fwcounter<COUNTER_BITS>, BIMODAL_TABLE_SIZE>> bimodal_table;
} // namespace

void O3_CPU::initialize_branch_predictor() {}

uint8_t O3_CPU::predict_branch(uint64_t ip)
{
  auto hash = ip % ::BIMODAL_TABLE_SIZE; // find the hash in the table with the instruction pointer
  auto value = ::bimodal_table[this][hash]; // Get the value from "this" bimodal table with the hash value we calculated

  return value.value() >= (value.maximum / 2); // if the value is greater than the maximum / 2 we return true else false 
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  // This code updates the table based on the previous result
  auto hash = ip % ::BIMODAL_TABLE_SIZE; // Find the hash for the bimodal table
  ::bimodal_table[this][hash] += taken ? 1 : -1; // If the value for that has was taken, then we increment our value in the table by 1, else subtracta
}