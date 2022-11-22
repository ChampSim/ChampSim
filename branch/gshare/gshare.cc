#include <algorithm>
#include <array>
#include <bitset>
#include <map>

#include "msl/fwcounter.h"
#include "ooo_cpu.h"

namespace
{
constexpr std::size_t GLOBAL_HISTORY_LENGTH = 14;
constexpr std::size_t COUNTER_BITS = 2;
constexpr std::size_t GS_HISTORY_TABLE_SIZE = 16384;

std::map<O3_CPU*, std::bitset<GLOBAL_HISTORY_LENGTH>> branch_history_vector;
std::map<O3_CPU*, std::array<champsim::msl::fwcounter<COUNTER_BITS>, GS_HISTORY_TABLE_SIZE>> gs_history_table;

std::size_t gs_table_hash(uint64_t ip, std::bitset<GLOBAL_HISTORY_LENGTH> bh_vector)
{
  std::size_t hash = bh_vector.to_ullong();
  hash ^= ip;
  hash ^= ip >> GLOBAL_HISTORY_LENGTH;
  hash ^= ip >> (GLOBAL_HISTORY_LENGTH * 2);

  return hash % GS_HISTORY_TABLE_SIZE;
}
} // namespace

void O3_CPU::initialize_branch_predictor() { std::cout << "CPU " << cpu << " GSHARE branch predictor" << std::endl; }

uint8_t O3_CPU::predict_branch(uint64_t ip)
{
  auto gs_hash = ::gs_table_hash(ip, ::branch_history_vector[this]);
  auto value = ::gs_history_table[this][gs_hash];
  return value.value() >= (value.maximum / 2);
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  auto gs_hash = gs_table_hash(ip, ::branch_history_vector[this]);
  ::gs_history_table[this][gs_hash] += taken ? 1 : -1;

  // update branch history vector
  ::branch_history_vector[this] <<= 1;
  ::branch_history_vector[this][0] = taken;
}
