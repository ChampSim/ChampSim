#include "gshare.h"

std::size_t gshare::gs_table_hash(champsim::address ip, std::bitset<GLOBAL_HISTORY_LENGTH> bh_vector)
{
  std::size_t hash = bh_vector.to_ullong();
  hash ^= ip.slice(champsim::static_extent<champsim::lg2(GS_HISTORY_TABLE_SIZE), 0>{}).to<std::size_t>();
  hash ^= ip.slice(champsim::static_extent<champsim::lg2(GS_HISTORY_TABLE_SIZE) + GLOBAL_HISTORY_LENGTH, GLOBAL_HISTORY_LENGTH>{}).to<std::size_t>();
  hash ^= ip.slice(champsim::static_extent<champsim::lg2(GS_HISTORY_TABLE_SIZE) + 2*GLOBAL_HISTORY_LENGTH, 2*GLOBAL_HISTORY_LENGTH>{}).to<std::size_t>();

  return hash % GS_HISTORY_TABLE_SIZE;
}

bool gshare::predict_branch(champsim::address ip)
{
  auto gs_hash = gs_table_hash(ip, branch_history_vector);
  auto value = gs_history_table[gs_hash];
  return value.value() >= (value.maximum / 2);
}

void gshare::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{
  auto gs_hash = gs_table_hash(ip, branch_history_vector);
  gs_history_table[gs_hash] += taken ? 1 : -1;

  // update branch history vector
  branch_history_vector <<= 1;
  branch_history_vector[0] = taken;
}
