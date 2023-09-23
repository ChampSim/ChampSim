#include <algorithm>
#include <array>
#include <bitset>
#include <map>

#include "msl/fwcounter.h"
#include "ooo_cpu.h"

namespace gshare
{
constexpr std::size_t GLOBAL_HISTORY_LENGTH = 14;
constexpr std::size_t COUNTER_BITS = 2;
constexpr std::size_t GS_HISTORY_TABLE_SIZE = 16384;

std::map<O3_CPU*, std::bitset<GLOBAL_HISTORY_LENGTH>> branch_history_vector;
std::map<O3_CPU*, std::array<champsim::msl::fwcounter<COUNTER_BITS>, GS_HISTORY_TABLE_SIZE>> gs_history_table;

std::size_t gs_table_hash(uint64_t ip, std::bitset<GLOBAL_HISTORY_LENGTH> bh_vector)
{
  // NOTE: in gshare the program counter is XORed with the branch history vector and then hashed
  std::size_t hash = bh_vector.to_ullong();
  hash ^= ip;
  hash ^= ip >> GLOBAL_HISTORY_LENGTH;
  hash ^= ip >> (GLOBAL_HISTORY_LENGTH * 2);

  return hash % GS_HISTORY_TABLE_SIZE;
}
} // namespace gshare

namespace bimodal
{
constexpr std::size_t BIMODAL_TABLE_SIZE = 16384;
constexpr std::size_t BIMODAL_PRIME = 16381;
constexpr std::size_t COUNTER_BITS = 2;

std::map<O3_CPU*, std::array<champsim::msl::fwcounter<COUNTER_BITS>, BIMODAL_TABLE_SIZE>> bimodal_table;
} // namespace bimodal

namespace hybrid
{
constexpr std::size_t HYBRID_TABLE_SIZE = 16384;
constexpr std::size_t COUNTER_BITS = 2;

std::map<O3_CPU*, std::array<champsim::msl::fwcounter<COUNTER_BITS>, HYBRID_TABLE_SIZE>> hybrid_table;
} // namespace hybrid

void O3_CPU::initialize_branch_predictor() {}

uint8_t O3_CPU::predict_branch(uint64_t ip)
{
  // predict from gshare
  auto gs_hash = gshare::gs_table_hash(ip, gshare::branch_history_vector[this]);
  auto gshare_value = gshare::gs_history_table[this][gs_hash];
  //   return value.value() >= (value.maximum / 2);

  // predict from bimodal
  auto hash = ip % bimodal::BIMODAL_PRIME;
  auto bimodal_value = bimodal::bimodal_table[this][hash];

  //   return value.value() >= (value.maximum / 2);

  // based on the confidence from the hybrid predictor select one of the result
  auto hybrid_value = hybrid::hybrid_table[this][hash];
  if (hybrid_value.value() >= hybrid_value.maximum / 2) {
    return bimodal_value.value() >= (bimodal_value.maximum / 2);
  } else {
    return gshare_value.value() >= (gshare_value.maximum / 2);
  }
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  // BIMODAL: update for bimodal
  auto hash = ip % bimodal::BIMODAL_PRIME;
  auto bimodal_previous = bimodal::bimodal_table[this][hash];
  auto bimodal_result = (bimodal_previous.value() >= (bimodal_previous.maximum / 2));

  bimodal::bimodal_table[this][hash] += taken ? 1 : -1;

  // GSHARE: update for gshare
  auto gs_hash = gshare::gs_table_hash(ip, gshare::branch_history_vector[this]);
  auto gshare_previous = gshare::gs_history_table[this][gs_hash];
  auto gshare_result = (gshare_previous.value() >= (gshare_previous.maximum / 2));
  gshare::gs_history_table[this][gs_hash] += taken ? 1 : -1;

  // GSHARE: update branch history vector
  gshare::branch_history_vector[this] <<= 1;
  gshare::branch_history_vector[this][0] = taken;

  // HYBRID:
  // now based on both the prediction, we need to update the hybrid predictor table
  // update only if one performed better than the other, else let it be the same.

  if (gshare_result != taken && bimodal_result == taken) {
    hybrid::hybrid_table[this][hash] += 1;
  } else if (gshare_result == taken && bimodal_result != taken) {
    hybrid::hybrid_table[this][hash] += -1;
  }
}
