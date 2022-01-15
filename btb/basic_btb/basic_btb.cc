
/*
 * This file implements a basic Branch Target Buffer (BTB) structure.
 * It uses a set-associative BTB to predict the targets of non-return branches,
 * and it uses a small Return Address Stack (RAS) to predict the target of
 * returns.
 */

#include "ooo_cpu.h"

#include <algorithm>
#include <bitset>
#include <deque>

#include "util.h"

constexpr std::size_t BASIC_BTB_SETS = 1024;
constexpr std::size_t BASIC_BTB_WAYS = 8;
constexpr std::size_t BASIC_BTB_INDIRECT_SIZE = 4096;
constexpr std::size_t BASIC_BTB_RAS_SIZE = 64;
constexpr std::size_t BASIC_BTB_CALL_INSTR_SIZE_TRACKERS = 1024;

struct BASIC_BTB_ENTRY
{
    uint64_t ip_tag = 0;
    uint64_t target = 0;
    bool always_taken = false;
    uint64_t last_cycle_used = 0;
};

std::map<O3_CPU*, std::array<BASIC_BTB_ENTRY, BASIC_BTB_SETS*BASIC_BTB_WAYS>> basic_btb;
std::map<O3_CPU*, std::array<uint64_t, BASIC_BTB_INDIRECT_SIZE>>              basic_btb_indirect;
std::map<O3_CPU*, std::bitset<lg2(BASIC_BTB_INDIRECT_SIZE)>>                  basic_btb_conditional_history;
std::map<O3_CPU*, std::deque<uint64_t>>                                       basic_btb_ras;
/*
 * The following structure identifies the size of call instructions so we can
 * find the target for a call's return, since calls may have different sizes.
 */
std::map<O3_CPU*, std::array<uint64_t, BASIC_BTB_CALL_INSTR_SIZE_TRACKERS>> basic_btb_call_instr_sizes;

void O3_CPU::initialize_btb() {
  std::cout << "Basic BTB sets: " << BASIC_BTB_SETS
            << " ways: " << BASIC_BTB_WAYS
            << " indirect buffer size: " << BASIC_BTB_INDIRECT_SIZE
            << " RAS size: " << BASIC_BTB_RAS_SIZE << std::endl;

  std::fill(std::begin(basic_btb[this]), std::end(basic_btb[this]), BASIC_BTB_ENTRY{});
  std::fill(std::begin(basic_btb_indirect[this]), std::end(basic_btb_indirect[this]), 0);
  std::fill(std::begin(basic_btb_call_instr_sizes[this]), std::end(basic_btb_call_instr_sizes[this]), 4);
  basic_btb_conditional_history[this] = 0;
}

std::pair<uint64_t, uint8_t> O3_CPU::btb_prediction(uint64_t ip, uint8_t branch_type)
{
    // add something to the RAS
    if (branch_type == BRANCH_DIRECT_CALL || branch_type == BRANCH_INDIRECT_CALL)
    {
        basic_btb_ras[this].push_back(ip);
        if (std::size(basic_btb_ras[this]) > BASIC_BTB_RAS_SIZE)
            basic_btb_ras[this].pop_front();
    }

    if (branch_type == BRANCH_RETURN)
    {
        if (std::empty(basic_btb_ras[this]))
            return {0, true};

        // peek at the top of the RAS and adjust for the size of the call instr
        auto target = basic_btb_ras[this].back();
        auto size = basic_btb_call_instr_sizes[this][target % std::size(basic_btb_call_instr_sizes[this])];

        return {target + size, true};
    }

    if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL))
    {
        auto hash = (ip >> 2) ^ basic_btb_conditional_history[this].to_ullong();
        return {basic_btb_indirect[this][hash % std::size(basic_btb_indirect[this])], true};
    }

    // use BTB for all other branches + direct calls
    auto set_idx = (ip >> 2) % BASIC_BTB_SETS;
    auto set_begin = std::next(std::begin(basic_btb[this]), set_idx*BASIC_BTB_WAYS);
    auto set_end   = std::next(set_begin, BASIC_BTB_WAYS);
    auto btb_entry = std::find_if(set_begin, set_end, [ip](auto x){ return x.ip_tag == ip; });

    // no prediction for this IP
    if (btb_entry == set_end)
        return {0, true};

    btb_entry->last_cycle_used = current_cycle;

    return {btb_entry->target, btb_entry->always_taken};
}

void O3_CPU::update_btb(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
    // updates for indirect branches
    if ((branch_type == BRANCH_INDIRECT) || (branch_type == BRANCH_INDIRECT_CALL))
    {
        auto hash = (ip >> 2) ^ basic_btb_conditional_history[this].to_ullong();
        basic_btb_indirect[this][hash % std::size(basic_btb_indirect[this])] = branch_target;
    }

    if (branch_type == BRANCH_CONDITIONAL)
    {
        basic_btb_conditional_history[this] <<= 1;
        basic_btb_conditional_history[this].set(0, taken);
    }

    if (branch_type == BRANCH_RETURN && !std::empty(basic_btb_ras[this]))
    {
        // recalibrate call-return offset
        // if our return prediction got us into the right ball park, but not the
        // exactly correct byte target, then adjust our call instr size tracker
        auto call_ip = basic_btb_ras[this].back();
        basic_btb_ras[this].pop_back();

        auto estimated_call_instr_size = (call_ip > branch_target) ? call_ip - branch_target : branch_target - call_ip;
        if (estimated_call_instr_size <= 10) {
            basic_btb_call_instr_sizes[this][call_ip % std::size(basic_btb_call_instr_sizes[this])] = estimated_call_instr_size;
        }
    }

    if (branch_type != BRANCH_INDIRECT && branch_type != BRANCH_INDIRECT_CALL)
    {
        auto set_idx = (ip >> 2) % BASIC_BTB_SETS;
        auto set_begin = std::next(std::begin(basic_btb[this]), set_idx*BASIC_BTB_WAYS);
        auto set_end   = std::next(set_begin, BASIC_BTB_WAYS);
        auto btb_entry = std::find_if(set_begin, set_end, [ip](auto x){ return x.ip_tag == ip; });

        // no prediction for this entry so far, so allocate one
        if (btb_entry == set_end && (branch_target != 0) && taken)
        {
            btb_entry = std::min_element(set_begin, set_end, [](auto x, auto y){ return x.last_cycle_used < y.last_cycle_used; });
            btb_entry->always_taken = true;
        }

        *btb_entry = {ip, branch_target, btb_entry->always_taken && taken, current_cycle};
    }
}

