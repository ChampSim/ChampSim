
/*
 * This file implements a basic Branch Target Buffer (BTB) structure.
 * It uses a set-associative BTB to predict the targets of non-return branches,
 * and it uses a small Return Address Stack (RAS) to predict the target of
 * returns.
 */

#include <algorithm>
#include <bitset>
#include <deque>
#include <sstream>
#include <iostream>
#include <map>
#include <fmt/ostream.h>


#include "msl/lru_table.h"
#include "bytecode_module.h"


namespace
{

constexpr bool USE_OPARGS = false;
constexpr std::size_t BYTECODE_BTB_SIZE = 1026;
constexpr int MAX_USAGE_VAL = 8;
constexpr std::size_t STARTING_USAGE_VAL = 4;
constexpr int MAX_CONFIDENCE = 3;
constexpr int STARTING_CONFIDENCE = 3;



struct btb_entry_t {
  int opcode = 0;
  int oparg = 0;
  int64_t jump = 0;
  bool valid = true;
  bool inner_entry = false;
  std::vector<btb_entry_t> inner_entries;

  int usage = STARTING_USAGE_VAL;
  int confidence = STARTING_CONFIDENCE;

  uint64_t hits{0}, misses{0};

  void update(int64_t correctJump) {
    if (valid == false) return;
    if (jump == 0) {
      jump = correctJump;
      return; 
    }
    if (correctJump == jump) {
      correctPrediction();
    } else {
      wrongPrediction(correctJump);
    }
  }

  void correctPrediction() { 
    if (confidence < MAX_CONFIDENCE) confidence++;
    if (usage < MAX_USAGE_VAL) usage++; 
    hits++;
  }

  void wrongPrediction(int64_t correctTarget) { 
    if (confidence > 1) confidence--;
    else {
      confidence = STARTING_CONFIDENCE;
      jump = correctTarget;
    }
    if (usage > 0) usage--;
    else valid = false;
    misses++;
  }

  btb_entry_t* findInnerEntry(int oparg) {
    if constexpr (!USE_OPARGS) return this;
    if (oparg == 0) return this;
    auto entry = std::find_if(inner_entries.begin(), inner_entries.end(), [oparg] (btb_entry_t btb_entry) { return btb_entry.oparg == oparg; });
    if (entry == inner_entries.end()) return nullptr;
    return &(*entry);
  }

  int64_t makePrediction(int oparg) {
    auto innerEntry = findInnerEntry(oparg);
    if (innerEntry == nullptr) {
      return (valid) ? jump : 0;
    }
    return (innerEntry->valid) ? innerEntry->jump : 0;
  }

  auto totalHitsAndMisses() const {
    uint64_t totalHits = hits;
    uint64_t totalMisses = misses;
    for (auto const &innerEntry : inner_entries) {
      totalHits += innerEntry.hits;
      totalMisses += innerEntry.misses;
    }
    return std::pair(totalHits, totalMisses);
  }

  double percentageHits() const {
    if (inner_entry) return 0;
    auto [totalHits, totalMisses] = totalHitsAndMisses();
    return 100 * (double) totalHits / ((double) totalHits + (double) totalMisses);
  }

};

std::vector<btb_entry_t> bytecode_BTB; 
/*
 * The following structure identifies the size of call instructions so we can
 * find the target for a call's return, since calls may have different sizes.
 */
} // namespace

btb_entry_t* findOuterEntry(int opcode) {
  auto entry = std::find_if(bytecode_BTB.begin(), bytecode_BTB.end(), [opcode](btb_entry_t entry) { return entry.opcode == opcode; });
  if (entry != bytecode_BTB.end()) return &(*entry);
  btb_entry_t newEntry;
  newEntry.opcode = opcode;
  bytecode_BTB.emplace_back(newEntry);
  return &bytecode_BTB.back();
}

int64_t BYTECODE_MODULE::btb_prediction(int opcode, int oparg)
{
  return findOuterEntry(opcode)->makePrediction(oparg);
}

void BYTECODE_MODULE::update_btb(int opcode, int oparg, int64_t correct_jump)
{
  auto outerEntry = findOuterEntry(opcode);
  auto innerEntry = outerEntry->findInnerEntry(oparg);
  if (innerEntry == nullptr) {
    if (correct_jump != outerEntry->jump) {
      btb_entry_t newEntry;
      newEntry.opcode = opcode;
      newEntry.oparg = oparg;
      newEntry.jump = correct_jump;
      outerEntry->inner_entries.emplace_back(newEntry);
    }
    outerEntry->update(correct_jump);
  } else {
    innerEntry->update(correct_jump);
  }
}

void BYTECODE_MODULE::printBTBs() {
    // Iterate over each module and its corresponding INDIRECT_BTB entries
    int moduleint = 0;
    fmt::print(stdout, "\n --- BYTECODE MODULE BTB STATS --- \n");
    uint64_t totalMisses{0}, totalHits{0};
    for (auto const &outer_entry : bytecode_BTB) {
      auto [hits, misses] = outer_entry.totalHitsAndMisses();
      totalMisses += misses;
      totalHits += hits;
    }
    auto totalPercentage = 100 * (double) totalHits / ((double) totalHits + (double) totalMisses);
    stats.BTB_PERCENTAGE = totalPercentage;

    fmt::print("BYTECODE BTB HITS: {}, MISS: {}, PERCENTAGE: {} \n", totalHits, totalMisses, totalPercentage);
    for (auto const &outer_entry : bytecode_BTB) {
        fmt::print(stdout, "Outer entry for opcode: {}, hits: {}, misses: {}, percentage: {}, prediction: {} \n \t", outer_entry.opcode, outer_entry.hits, outer_entry.misses, outer_entry.percentageHits(), outer_entry.jump);
        for (auto const &inner_entry : outer_entry.inner_entries) {
          fmt::print(stdout, "  [arg: {}, h: {}, m: {}, j: {}] ", inner_entry.opcode, inner_entry.hits, inner_entry.misses, inner_entry.jump);
        }
        fmt::print(stdout, "\n");
    }
    fmt::print(stdout, "---------------------------------- \n\n");
}
