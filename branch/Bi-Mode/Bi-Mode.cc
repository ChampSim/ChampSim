#include <map>
#include <bitset>

#include "msl/fwcounter.h"
#include "ooo_cpu.h"

// Architecture is from the paper below
// https://people.eecs.berkeley.edu/~kubitron/courses/cs152-S04/handouts/papers/p4-lee.pdf
// We split the second level table into two halves 
namespace
{
  // 3*32k bits

constexpr int TABLE_SIZE = 16384;
constexpr int CHOICE_TABLE_SIZE = 4096;
constexpr int COUNTER_BITS = 4;
constexpr int GLOBAL_HISTORY_LENGTH = 14; // How long the history register is 

std::bitset<GLOBAL_HISTORY_LENGTH > GLOBAL_HISTORY;

std::map<O3_CPU*, std::array<champsim::msl::fwcounter<COUNTER_BITS>, CHOICE_TABLE_SIZE>> choice_predictor;
std::map<O3_CPU*, std::array<champsim::msl::fwcounter<COUNTER_BITS>, TABLE_SIZE>> direction_table1;
std::map<O3_CPU*, std::array<champsim::msl::fwcounter<COUNTER_BITS>, TABLE_SIZE>> direction_table2;
} // namespace

void O3_CPU::initialize_branch_predictor() {}

uint8_t O3_CPU::predict_branch(uint64_t ip)
{
  /// 
  champsim::msl::fwcounter<COUNTER_BITS> value = ::direction_table1[this][GLOBAL_HISTORY.to_ullong()];
  // We start by hashing the choice predictor with the ip
  auto hash = ip % ::CHOICE_TABLE_SIZE;
  auto choice = ::choice_predictor[this][hash];
  // We use the choice predictor to decide which direction table we will use 
  if (choice.value()) // Use table 1
    value = ::direction_table1[this][GLOBAL_HISTORY.to_ullong()];
  else // Use table 2
    value = ::direction_table2[this][GLOBAL_HISTORY.to_ullong()];

  // return the value from whichever table was selected 
  return value.value() >= (value.maximum / 2);
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{

  // Update the choice predictor 
  auto hash = ip % ::CHOICE_TABLE_SIZE;
  ::choice_predictor[this][hash] += taken ? 1 : -1;

  // Update whichever direction table was used 
  if (taken) // Use Table 1
    ::direction_table1[this][GLOBAL_HISTORY.to_ullong()] += taken ? 1 : -1;
  else // Use table 2
    ::direction_table2[this][GLOBAL_HISTORY.to_ullong()] += taken ? 1 : -1;

  // Update the Global History 
  GLOBAL_HISTORY >>= 1; // Shift the history register to the left to remove the least recent data 
  GLOBAL_HISTORY[GLOBAL_HISTORY_LENGTH-1] = taken; // insert the most recent data into the history register 
}
