
/*
 * This file implements a basic Branch Target Buffer (BTB) structure.
 * It uses a set-associative BTB to predict the targets of non-return branches,
 * and it uses a small Return Address Stack (RAS) to predict the target of
 * returns.
 */

#include <algorithm>
#include <bitset>
#include <deque>
#include <map>

#include "ooo_cpu.h"
#include "util.h"

enum branch_info {
  BRANCH_INFO_INDIRECT,
  BRANCH_INFO_RETURN,
  BRANCH_INFO_ALWAYS_TAKEN,
  BRANCH_INFO_CONDITIONAL,
};

struct BASIC_BTB_ENTRY {
  uint64_t ip_tag;
  uint64_t target;
  uint8_t branch_info;
  uint64_t lru;
};

constexpr std::size_t BTB_SET = 1024;
constexpr std::size_t BTB_WAY = 8;
constexpr std::size_t BTB_INDIRECT_SIZE = 4096;
constexpr std::size_t RAS_SIZE = 64;
constexpr std::size_t CALL_SIZE_TRACKERS = 1024;

struct btb_entry_t {
  uint64_t ip_tag = 0;
  uint64_t target = 0;
  uint8_t branch_info = BRANCH_INFO_ALWAYS_TAKEN;
  uint64_t last_cycle_used = 0;
};

std::map<O3_CPU*, std::array<btb_entry_t, BTB_SET * BTB_WAY>> BTB;
std::map<O3_CPU*, std::array<uint64_t, BTB_INDIRECT_SIZE>> INDIRECT_BTB;
std::map<O3_CPU*, std::bitset<lg2(BTB_INDIRECT_SIZE)>> CONDITIONAL_HISTORY;
std::map<O3_CPU*, std::deque<uint64_t>> RAS;
/*
 * The following structure identifies the size of call instructions so we can
 * find the target for a call's return, since calls may have different sizes.
 */
std::map<O3_CPU*, std::array<uint64_t, CALL_SIZE_TRACKERS>> CALL_SIZE;

void O3_CPU::initialize_btb()
{
  std::cout << "Basic BTB sets: " << BTB_SET << " ways: " << BTB_WAY << " indirect buffer size: " << std::size(INDIRECT_BTB[this]) << " RAS size: " << RAS_SIZE
            << std::endl;

  std::fill(std::begin(BTB[this]), std::end(BTB[this]), btb_entry_t{});
  std::fill(std::begin(INDIRECT_BTB[this]), std::end(INDIRECT_BTB[this]), 0);
  std::fill(std::begin(CALL_SIZE[this]), std::end(CALL_SIZE[this]), 4);
  CONDITIONAL_HISTORY[this] = 0;
}

std::pair<uint64_t, uint8_t> O3_CPU::btb_prediction(uint64_t ip)
{
  // use BTB for all other branches + direct calls
  auto set_idx = (ip >> 2) % BTB_SET;
  auto set_begin = std::next(std::begin(BTB[this]), set_idx * BTB_WAY);
  auto set_end = std::next(set_begin, BTB_WAY);
  auto btb_entry = std::find_if(set_begin, set_end, [ip](auto x) { return x.ip_tag == ip; });

  // no prediction for this IP
  if (btb_entry == set_end)
    return {0, true};

  if (btb_entry->branch_info == BRANCH_INFO_RETURN) {
    if (std::empty(RAS[this]))
      return {0, true};

    // peek at the top of the RAS and adjust for the size of the call instr
    auto target = RAS[this].back();
    auto size = CALL_SIZE[this][target % std::size(CALL_SIZE[this])];

    return {target + size, true};
  }

  if (btb_entry->branch_info == BRANCH_INFO_INDIRECT) {
    auto hash = (ip >> 2) ^ CONDITIONAL_HISTORY[this].to_ullong();
    return {INDIRECT_BTB[this][hash % std::size(INDIRECT_BTB[this])], true};
  }

  btb_entry->last_cycle_used = current_cycle;

  return {btb_entry->target, btb_entry->branch_info != BRANCH_INFO_CONDITIONAL};
}

void O3_CPU::update_btb(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  // add something to the RAS
  if (branch_type == BRANCH_DIRECT_CALL || branch_type == BRANCH_INDIRECT_CALL) {
    RAS[this].push_back(ip);
    if (std::size(RAS[this]) > RAS_SIZE)
      RAS[this].pop_front();
  }

  // updates for indirect branches
  if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL)) {
    auto hash = (ip >> 2) ^ CONDITIONAL_HISTORY[this].to_ullong();
    INDIRECT_BTB[this][hash % std::size(INDIRECT_BTB[this])] = branch_target;
  }

  if (branch_type == BRANCH_CONDITIONAL) {
    CONDITIONAL_HISTORY[this] <<= 1;
    CONDITIONAL_HISTORY[this].set(0, taken);
  }

  if (branch_type == BRANCH_RETURN && !std::empty(RAS[this])) {
    // recalibrate call-return offset if our return prediction got us close, but not exact
    auto call_ip = RAS[this].back();
    RAS[this].pop_back();

    auto estimated_call_instr_size = (call_ip > branch_target) ? call_ip - branch_target : branch_target - call_ip;
    if (estimated_call_instr_size <= 10) {
      CALL_SIZE[this][call_ip % std::size(CALL_SIZE[this])] = estimated_call_instr_size;
    }
  }

  auto set_idx = (ip >> 2) % BTB_SET;
  auto set_begin = std::next(std::begin(BTB[this]), set_idx * BTB_WAY);
  auto set_end = std::next(set_begin, BTB_WAY);
  auto btb_entry = std::find_if(set_begin, set_end, [ip](auto x) { return x.ip_tag == ip; });

  // no prediction for this entry so far, so allocate one
  if (btb_entry == set_end && (branch_target != 0) && taken) {
    btb_entry = std::min_element(set_begin, set_end, [](auto x, auto y) { return x.last_cycle_used < y.last_cycle_used; });
    *btb_entry = {};
  }

  // update btb entry
  btb_entry->ip_tag = ip;
  btb_entry->target = branch_target;
  if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL)) {
    btb_entry->branch_info = BRANCH_INFO_INDIRECT;
  } else if (branch_type == BRANCH_RETURN) {
    btb_entry->branch_info = BRANCH_INFO_RETURN;
  } else if (!taken) {
    btb_entry->branch_info = BRANCH_INFO_CONDITIONAL;
  }
  btb_entry->last_cycle_used = current_cycle;
}
