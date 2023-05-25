#include "gshare.h"

std::size_t gshare::gs_table_hash(uint64_t ip, std::bitset<GLOBAL_HISTORY_LENGTH> bh_vector)
{
  std::size_t hash = bh_vector.to_ullong();
  hash ^= ip;
  hash ^= ip >> GLOBAL_HISTORY_LENGTH;
  hash ^= ip >> (GLOBAL_HISTORY_LENGTH * 2);

  return hash % GS_HISTORY_TABLE_SIZE;
}

bool gshare::predict_branch(uint64_t ip)
{
  auto gs_hash = gs_table_hash(ip, branch_history_vector);
  auto value = gs_history_table[gs_hash];
  return value.value() >= (value.maximum / 2);
}

void gshare::last_branch_result(uint64_t ip, uint64_t branch_target, bool taken, uint8_t branch_type)
{
  auto gs_hash = gs_table_hash(ip, branch_history_vector);
  gs_history_table[gs_hash] += taken ? 1 : -1;

  // update branch history vector
  branch_history_vector <<= 1;
  branch_history_vector[0] = taken;
}
