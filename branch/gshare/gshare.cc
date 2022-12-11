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

std::size_t gs_table_hash(champsim::address ip, std::bitset<GLOBAL_HISTORY_LENGTH> bh_vector)
{
  std::size_t hash = bh_vector.to_ullong();
  hash ^= ip.slice<champsim::lg2(GS_HISTORY_TABLE_SIZE), 0>().to<std::size_t>();
  hash ^= ip.slice<champsim::lg2(GS_HISTORY_TABLE_SIZE) + GLOBAL_HISTORY_LENGTH, GLOBAL_HISTORY_LENGTH>().to<std::size_t>();
  hash ^= ip.slice<champsim::lg2(GS_HISTORY_TABLE_SIZE) + 2*GLOBAL_HISTORY_LENGTH, 2*GLOBAL_HISTORY_LENGTH>().to<std::size_t>();

  return hash % GS_HISTORY_TABLE_SIZE;
}
} // namespace

void O3_CPU::initialize_branch_predictor() { std::cout << "CPU " << cpu << " GSHARE branch predictor" << std::endl; }

bool O3_CPU::predict_branch(champsim::address ip)
{
  auto gs_hash = ::gs_table_hash(ip, ::branch_history_vector[this]);
  auto value = ::gs_history_table[this][gs_hash];
  return value.value() >= (value.maximum / 2);
}

void O3_CPU::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{
  auto gs_hash = gs_table_hash(ip, ::branch_history_vector[this]);
  ::gs_history_table[this][gs_hash] += taken ? 1 : -1;

  // update branch history vector
  ::branch_history_vector[this] <<= 1;
  ::branch_history_vector[this][0] = taken;
}
