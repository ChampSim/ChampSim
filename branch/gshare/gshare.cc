#include <algorithm>
#include <array>
#include <bitset>
#include <map>

#include "ooo_cpu.h"

constexpr std::size_t GLOBAL_HISTORY_LENGTH = 14;
constexpr std::size_t COUNTER_BITS = 2;
constexpr std::size_t GS_HISTORY_TABLE_SIZE = 16384;

std::map<O3_CPU*, std::bitset<GLOBAL_HISTORY_LENGTH>> branch_history_vector;
std::map<O3_CPU*, std::array<int, GS_HISTORY_TABLE_SIZE>> gs_history_table;

constexpr int COUNTER_MAX = (1 << COUNTER_BITS) - 1;
constexpr int COUNTER_MIN = 0;
constexpr int COUNTER_THRESH = (COUNTER_MAX + COUNTER_MIN + 1) / 2;

void O3_CPU::initialize_branch_predictor()
{
  std::cout << "CPU " << cpu << " GSHARE branch predictor" << std::endl;

  std::fill(std::begin(gs_history_table[this]), std::end(gs_history_table[this]), COUNTER_THRESH); // weakly taken
}

std::size_t gs_table_hash(uint64_t ip, std::bitset<GLOBAL_HISTORY_LENGTH> bh_vector)
{
  std::size_t hash = bh_vector.to_ullong();
  hash ^= ip;
  hash ^= ip >> GLOBAL_HISTORY_LENGTH;
  hash ^= ip >> (GLOBAL_HISTORY_LENGTH * 2);

  return hash % GS_HISTORY_TABLE_SIZE;
}

uint8_t O3_CPU::predict_branch(uint64_t ip, uint64_t predicted_target, uint8_t always_taken, uint8_t branch_type)
{
  auto gs_hash = gs_table_hash(ip, branch_history_vector[this]);
  return gs_history_table[this][gs_hash] >= COUNTER_THRESH;
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  auto gs_hash = gs_table_hash(ip, branch_history_vector[this]);

  if (taken)
    gs_history_table[this][gs_hash] = std::min(gs_history_table[this][gs_hash] + 1, COUNTER_MAX);
  else
    gs_history_table[this][gs_hash] = std::max(gs_history_table[this][gs_hash] - 1, COUNTER_MIN);

  // update branch history vector
  branch_history_vector[this] <<= 1;
  branch_history_vector[this][0] = taken;
}
