#include <map>
#include<iostream>
#include "msl/fwcounter.h"
#include "ooo_cpu.h"




// Methodology For this predictor
// Table of 1 bit entries
// Indexed by lower bits of the Program Counter
// Branch is 1 if taken last time, 0 if not taken last time
// No address tags ? not really sure what this means yet 
// Helps when branch condition is computer later than branch target 
namespace
{

constexpr std::size_t TABLE_SIZE = 16384;
constexpr std::size_t COUNTER_BITS = 1;
std::map<O3_CPU*, std::array<champsim::msl::fwcounter<COUNTER_BITS>,TABLE_SIZE>> history_table;
} 

void O3_CPU::initialize_branch_predictor() {}

uint8_t O3_CPU::predict_branch(uint64_t ip)
{
  auto hash = ip % ::TABLE_SIZE;
  auto value = ::history_table[this][hash]; 

  return value.value();
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  auto hash = ip % ::TABLE_SIZE;
  ::history_table[this][hash] = taken;
}
