/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <numeric>
#include <sstream>
#include <utility>
#include <vector>

#include "stats_printer.h"
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <format>

void champsim::plain_printer::print(O3_CPU::stats_type stats)
{
  constexpr std::array<std::pair<std::string_view, std::size_t>, 6> types{
      {std::pair{"BRANCH_DIRECT_JUMP", BRANCH_DIRECT_JUMP}, std::pair{"BRANCH_INDIRECT", BRANCH_INDIRECT}, std::pair{"BRANCH_CONDITIONAL", BRANCH_CONDITIONAL},
       std::pair{"BRANCH_DIRECT_CALL", BRANCH_DIRECT_CALL}, std::pair{"BRANCH_INDIRECT_CALL", BRANCH_INDIRECT_CALL},
       std::pair{"BRANCH_RETURN", BRANCH_RETURN}}};

  auto total_branch = std::ceil(
      std::accumulate(std::begin(types), std::end(types), 0ll, [tbt = stats.total_branch_types](auto acc, auto next) { return acc + tbt[next.second]; }));
  auto total_mispredictions = std::ceil(
      std::accumulate(std::begin(types), std::end(types), 0ll, [btm = stats.branch_type_misses](auto acc, auto next) { return acc + btm[next.second]; }));

  fmt::print(stream, "\n{} cumulative IPC: {:.4g} instructions: {} cycles: {}\n", stats.name, std::ceil(stats.instrs()) / std::ceil(stats.cycles()),
             stats.instrs(), stats.cycles());
  fmt::print(stream, "{} Branch Prediction Accuracy: {:.4g}% MPKI: {:.4g} Average ROB Occupancy at Mispredict: {:.4g}\n", stats.name,
             (100.0 * std::ceil(total_branch - total_mispredictions)) / total_branch, (1000.0 * total_mispredictions) / std::ceil(stats.instrs()),
             std::ceil(stats.total_rob_occupancy_at_branch_mispredict) / total_mispredictions);

  std::vector<double> mpkis;
  std::transform(std::begin(stats.branch_type_misses), std::end(stats.branch_type_misses), std::back_inserter(mpkis),
                 [instrs = stats.instrs()](auto x) { return 1000.0 * std::ceil(x) / std::ceil(instrs); });

  fmt::print(stream, "Branch type MPKI\n");
  for (auto [str, idx] : types)
    fmt::print(stream, "{}: {:.3}\n", str, mpkis[idx]);
  fmt::print(stream, "Seen bytecodes: {}\n", stats.bytecodes_seen);
  fmt::print(stream, "Average bytecode length (ins): {}\n", stats.avgInstrPrBytecode());
  fmt::print(stream, "Average bytecode length buckets (#ins, #freq): \n");
  for (auto const bytecodeLength : stats.bytecode_lengths) {
   fmt::print(stream, " [{} , {}]", bytecodeLength.first * 10, bytecodeLength.second);
  }
  fmt::print(stream, "\n");

  fmt::print(stream, "Length before dispatch:"); 
  for (auto const lengths : stats.lengthBetweenBytecodeAndTable) {
    fmt::print(stream, " [l: {}, f: {}]", lengths.first * 5, lengths.second);
  }
  fmt::print(stream, "\n");

  fmt::print(stream, "Lengths before jump after prediction:");
  for (auto const lengths : stats.lengthBetweenPredictionAndJump) {
    fmt::print(stream, " [l: {}, f: {}]", lengths.first * 5, lengths.second);
  }
  fmt::print(stream, "\n");

  if constexpr (SKIP_DISPATCH) {
    fmt::print(stream, "Unclear bytecodeLoads IPs: ");
    for (auto const ip : stats.unclearBytecodeLoads) {
      fmt::print(stream, " ip: {} ", ip);
    }
    fmt::print(stream, "\n");

    fmt::print(stream, "Clear IPs: \n");
    for (auto const ip : stats.clearBytecodeLoads) {
      fmt::print(stream, " ip: {} ", ip);
    }
    fmt::print(stream, "\n");

    fmt::print(stream, "Bytecodes of unclear IPs: \n");
    for (auto const bytecode : stats.unclearBytecodes) {
      fmt::print(stream, " bytecode: {} times: {}", getOpcodeName(bytecode.first), bytecode.second);
    }
    fmt::print(stream, "\n");

    fmt::print(stream, "Bytecodes clear: \n");
    for (auto const bytecode : stats.clearBytecodes) {
      fmt::print(stream, " bytecode: {} times: {} ", getOpcodeName(bytecode.first), bytecode.second);
    }
    fmt::print(stream, "\n");

    fmt::print(stream, "BYTECODE BUFFER stats, hits: {} miss: {}, percentage hits: {}, average miss cycles: {}, prefetches: {} \n\n", stats.bb_stats.hits, stats.bb_stats.miss, (100 * stats.bb_stats.hits) / (stats.bb_stats.hits + stats.bb_stats.miss), stats.bb_stats.averageWaitTime(), stats.bb_stats.prefetches);

    fmt::print(stream, "BYTECODE BTB - strong: {}, weak: {}, wrong: {} \n", stats.bb_mod.strongly_correct, stats.bb_mod.weakly_correct, stats.bb_mod.wrong);

    fmt::print(stream, "Bytecode jump predicitons, correct: {} wrong {} \n", stats.correctBytecodeJumpPredictions, stats.wrongBytecodeJumpPredictions);
  }

}

void champsim::plain_printer::print(CACHE::stats_type stats)
{
  constexpr std::array<std::pair<std::string_view, std::size_t>, 5> types{
      {std::pair{"LOAD", champsim::to_underlying(access_type::LOAD)}, std::pair{"RFO", champsim::to_underlying(access_type::RFO)},
       std::pair{"PREFETCH", champsim::to_underlying(access_type::PREFETCH)}, std::pair{"WRITE", champsim::to_underlying(access_type::WRITE)},
       std::pair{"TRANSLATION", champsim::to_underlying(access_type::TRANSLATION)}}};

  for (std::size_t cpu = 0; cpu < NUM_CPUS; ++cpu) {
    uint64_t TOTAL_HIT = 0, TOTAL_MISS = 0;
    for (const auto& type : types) {
      TOTAL_HIT += stats.hits.at(type.second).at(cpu);
      TOTAL_MISS += stats.misses.at(type.second).at(cpu);
    }

    fmt::print(stream, "{} TOTAL ACCESS: {:10d} HIT: {:10d} MISS: {:10d}\n", stats.name, TOTAL_HIT + TOTAL_MISS, TOTAL_HIT, TOTAL_MISS);
    for (const auto& type : types) {
      fmt::print(stream, "{} {:<12s} ACCESS: {:10d} HIT: {:10d} MISS: {:10d}\n", stats.name, type.first,
                 stats.hits[type.second][cpu] + stats.misses[type.second][cpu], stats.hits[type.second][cpu], stats.misses[type.second][cpu]);
      fmt::print(stream, "{} {:<12s} BYTECODE ACCESS: {:10d} HIT: {:10d} MISS: {:10d}\n", stats.name, type.first,
                 stats.bytecode_hits[type.second][cpu] + stats.bytecode_miss[type.second][cpu], stats.bytecode_hits[type.second][cpu], stats.bytecode_miss[type.second][cpu]);
      fmt::print(stream, "{} {:<12s} DISPATCH TABLE ACCESS: {:10d} HIT: {:10d} MISS: {:10d}\n", stats.name, type.first,
                 stats.table_hits[type.second][cpu] + stats.table_miss[type.second][cpu], stats.table_hits[type.second][cpu], stats.table_miss[type.second][cpu]);
    }

    fmt::print(stream, "{} PREFETCH REQUESTED: {:10} ISSUED: {:10} USEFUL: {:10} USELESS: {:10}\n", stats.name, stats.pf_requested, stats.pf_issued,
               stats.pf_useful, stats.pf_useless);

    fmt::print(stream, "{} AVERAGE MISS LATENCY: {:.4g} cycles\n", stats.name, stats.avg_miss_latency);
    
    fmt::print(stream, "{} AVERAGE BYTECODE MISS LATENCY: {:.4g} cycles\n", stats.name, stats.avg_miss_latency_bytecode);
    
    fmt::print(stream, "{} AVERAGE DISPATCH TABLE MISS LATENCY: {:.4g} cycles\n", stats.name, stats.avg_miss_latency_table);

    fmt::print(stream, "{} AVERAGE BYTECODE FILL : {} %\n", stats.name, stats.bytecode_occupancy[stats.name].first/(float) stats.bytecode_occupancy[stats.name].second);

    fmt::print(stream, "\n");
  }
}

void champsim::plain_printer::print(DRAM_CHANNEL::stats_type stats)
{
  fmt::print(stream, "\n{} RQ ROW_BUFFER_HIT: {:10}\n  ROW_BUFFER_MISS: {:10}\n", stats.name, stats.RQ_ROW_BUFFER_HIT, stats.RQ_ROW_BUFFER_MISS);
  if (stats.dbus_count_congested > 0)
    fmt::print(stream, " AVG DBUS CONGESTED CYCLE: {:.4g}\n", std::ceil(stats.dbus_cycle_congested) / std::ceil(stats.dbus_count_congested));
  else
    fmt::print(stream, " AVG DBUS CONGESTED CYCLE: -\n");
  fmt::print(stream, "WQ ROW_BUFFER_HIT: {:10}\n  ROW_BUFFER_MISS: {:10}\n  FULL: {:10}\n", stats.name, stats.WQ_ROW_BUFFER_HIT, stats.WQ_ROW_BUFFER_MISS,
             stats.WQ_FULL);
}

void champsim::plain_printer::print(champsim::phase_stats& stats)
{
  fmt::print(stream, "=== {} ===\n", stats.name);

  int i = 0;
  for (auto tn : stats.trace_names)
    fmt::print(stream, "CPU {} runs {}", i++, tn);

  if (NUM_CPUS > 1) {
    fmt::print(stream, "\nTotal Simulation Statistics (not including warmup)\n");

    for (const auto& stat : stats.sim_cpu_stats)
      print(stat);

    for (const auto& stat : stats.sim_cache_stats)
      print(stat);
  }

  fmt::print(stream, "\nRegion of Interest Statistics\n");

  for (const auto& stat : stats.roi_cpu_stats)
    print(stat);

  for (const auto& stat : stats.roi_cache_stats)
    print(stat);

  fmt::print(stream, "\nDRAM Statistics\n");
  for (const auto& stat : stats.roi_dram_stats)
    print(stat);
}

void champsim::plain_printer::print(std::vector<phase_stats>& stats)
{
  for (auto p : stats)
    print(p);
}

bool champsim::plain_printer::checkInnerSets(const std::map<int, std::map<int, std::map<int64_t, uint64_t>>>& bytecodeJumpMap, int outerKey) {
    // Find the outer map entry with the specified outerKey
    auto outerIt = bytecodeJumpMap.find(outerKey);
    if (outerIt == bytecodeJumpMap.end()) {
        std::cout << "Outer key not found." << std::endl;
        return false; // Outer key not found
    }

    const auto& innerMap = outerIt->second;
    if (innerMap.empty()) {
        std::cout << "No inner maps to compare." << std::endl;
        return false; // No inner maps available
    }

    // Get an iterator to the first set in the inner map
    auto setIt = innerMap.begin()->second;
    // Compare all sets in the inner map to the first set
    for (const auto& entry : innerMap) {
      if (setIt.size() != entry.second.size()) {
          return false; // Maps can't have the same keys if their sizes differ
      }

      if(!std::equal(setIt.begin(), setIt.end(), entry.second.begin(), [](const auto& a, const auto& b) {
        return a.first == b.first;
      })) {
        return false;
      }
    }

    return true; // All sets are the same
}

template<typename K, typename V>
bool haveSameKeys(const std::map<K, V>& map1, const std::map<K, V>& map2) {
    if (map1.size() != map2.size()) {
        return false; // Maps can't have the same keys if their sizes differ
    }

    return std::equal(map1.begin(), map1.end(), map2.begin(),
                      [](const auto& a, const auto& b) {
                          return a.first == b.first;
                      });
}

std::string champsim::plain_printer::getOpcodeName(int opcode) {
    switch(opcode) {
        case 0: return "CACHE";
        case 1: return "POP_TOP";
        case 2: return "PUSH_NULL";
        case 3: return "BINARY_OP_ADAPTIVE";
        case 4: return "BINARY_OP_ADD_FLOAT";
        case 5: return "BINARY_OP_ADD_INT";
        case 6: return "BINARY_OP_ADD_UNICODE";
        case 7: return "BINARY_OP_INPLACE_ADD_UNICODE";
        case 8: return "BINARY_OP_MULTIPLY_FLOAT";
        case 9: return "NOP";
        case 10: return "UNARY_POSITIVE";
        case 11: return "UNARY_NEGATIVE";
        case 12: return "UNARY_NOT";
        case 13: return "BINARY_OP_MULTIPLY_INT";
        case 14: return "BINARY_OP_SUBTRACT_FLOAT";
        case 15: return "UNARY_INVERT";
        case 16: return "BINARY_OP_SUBTRACT_INT";
        case 17: return "BINARY_SUBSCR_ADAPTIVE";
        case 18: return "BINARY_SUBSCR_DICT";
        case 19: return "BINARY_SUBSCR_GETITEM";
        case 20: return "BINARY_SUBSCR_LIST_INT";
        case 21: return "BINARY_SUBSCR_TUPLE_INT";
        case 22: return "CALL_ADAPTIVE";
        case 23: return "CALL_PY_EXACT_ARGS";
        case 24: return "CALL_PY_WITH_DEFAULTS";
        case 25: return "BINARY_SUBSCR";
        case 26: return "COMPARE_OP_ADAPTIVE";
        case 27: return "COMPARE_OP_FLOAT_JUMP";
        case 28: return "COMPARE_OP_INT_JUMP";
        case 29: return "COMPARE_OP_STR_JUMP";
        case 30: return "GET_LEN";
        case 31: return "MATCH_MAPPING";
        case 32: return "MATCH_SEQUENCE";
        case 33: return "MATCH_KEYS";
        case 34: return "EXTENDED_ARG_QUICK";
        case 35: return "PUSH_EXC_INFO";
        case 36: return "CHECK_EXC_MATCH";
        case 37: return "CHECK_EG_MATCH";
        case 38: return "JUMP_BACKWARD_QUICK";
        case 39: return "LOAD_ATTR_ADAPTIVE";
        case 40: return "LOAD_ATTR_INSTANCE_VALUE";
        case 41: return "LOAD_ATTR_MODULE";
        case 42: return "LOAD_ATTR_SLOT";
        case 43: return "LOAD_ATTR_WITH_HINT";
        case 44: return "LOAD_CONST__LOAD_FAST";
        case 45: return "LOAD_FAST__LOAD_CONST";
        case 46: return "LOAD_FAST__LOAD_FAST";
        case 47: return "LOAD_GLOBAL_ADAPTIVE";
        case 48: return "LOAD_GLOBAL_BUILTIN";
        case 49: return "WITH_EXCEPT_START";
        case 50: return "GET_AITER";
        case 51: return "GET_ANEXT";
        case 52: return "BEFORE_ASYNC_WITH";
        case 53: return "BEFORE_WITH";
        case 54: return "END_ASYNC_FOR";
        case 55: return "LOAD_GLOBAL_MODULE";
        case 56: return "LOAD_METHOD_ADAPTIVE";
        case 57: return "LOAD_METHOD_CLASS";
        case 58: return "LOAD_METHOD_MODULE";
        case 59: return "LOAD_METHOD_NO_DICT";
        case 60: return "STORE_SUBSCR";
        case 61: return "DELETE_SUBSCR";
        case 62: return "LOAD_METHOD_WITH_DICT";
        case 63: return "LOAD_METHOD_WITH_VALUES";
        case 64: return "PRECALL_ADAPTIVE";
        case 65: return "PRECALL_BOUND_METHOD";
        case 66: return "PRECALL_BUILTIN_CLASS";
        case 67: return "PRECALL_BUILTIN_FAST_WITH_KEYWORDS";
        case 68: return "GET_ITER";
        case 69: return "GET_YIELD_FROM_ITER";
        case 70: return "PRINT_EXPR";
        case 71: return "LOAD_BUILD_CLASS";
        case 72: return "PRECALL_METHOD_DESCRIPTOR_FAST_WITH_KEYWORDS";
        case 73: return "PRECALL_NO_KW_BUILTIN_FAST";
        case 74: return "LOAD_ASSERTION_ERROR";
        case 75: return "RETURN_GENERATOR";
        case 76: return "PRECALL_NO_KW_BUILTIN_O";
        case 77: return "PRECALL_NO_KW_ISINSTANCE";
        case 78: return "PRECALL_NO_KW_LEN";
        case 79: return "PRECALL_NO_KW_LIST_APPEND";
        case 80: return "PRECALL_NO_KW_METHOD_DESCRIPTOR_FAST";
        case 81: return "PRECALL_NO_KW_METHOD_DESCRIPTOR_NOARGS";
        case 82: return "LIST_TO_TUPLE";
        case 83: return "RETURN_VALUE";
        case 84: return "IMPORT_STAR";
        case 85: return "SETUP_ANNOTATIONS";
        case 86: return "YIELD_VALUE";
        case 87: return "ASYNC_GEN_WRAP";
        case 88: return "PREP_RERAISE_STAR";
        case 89: return "POP_EXCEPT";
        case 90: return "HAVE_ARGUMENT";
        case 91: return "DELETE_NAME";
        case 92: return "UNPACK_SEQUENCE";
        case 93: return "FOR_ITER";
        case 94: return "UNPACK_EX";
        case 95: return "STORE_ATTR";
        case 96: return "DELETE_ATTR";
        case 97: return "STORE_GLOBAL";
        case 98: return "DELETE_GLOBAL";
        case 99: return "SWAP";
        case 100: return "LOAD_CONST";
        case 101: return "LOAD_NAME";
        case 102: return "BUILD_TUPLE";
        case 103: return "BUILD_LIST";
        case 104: return "BUILD_SET";
        case 105: return "BUILD_MAP";
        case 106: return "LOAD_ATTR";
        case 107: return "COMPARE_OP";
        case 108: return "IMPORT_NAME";
        case 109: return "IMPORT_FROM";
        case 110: return "JUMP_FORWARD";
        case 111: return "JUMP_IF_FALSE_OR_POP";
        case 112: return "JUMP_IF_TRUE_OR_POP";
        case 113: return "PRECALL_NO_KW_METHOD_DESCRIPTOR_O";
        case 114: return "POP_JUMP_FORWARD_IF_FALSE";
        case 115: return "POP_JUMP_FORWARD_IF_TRUE";
        case 116: return "LOAD_GLOBAL";
        case 117: return "IS_OP";
        case 118: return "CONTAINS_OP";
        case 119: return "RERAISE";
        case 120: return "COPY";
        case 121: return "PRECALL_NO_KW_STR_1";
        case 122: return "BINARY_OP";
        case 123: return "SEND";
        case 124: return "LOAD_FAST";
        case 125: return "STORE_FAST";
        case 126: return "DELETE_FAST";
        case 127: return "POP_JUMP_FORWARD_IF_NOT_NONE";
        case 128: return "POP_JUMP_FORWARD_IF_NONE";
        case 129: return "RAISE_VARARGS";
        case 130: return "GET_AWAITABLE";
        case 131: return "MAKE_FUNCTION";
        case 132: return "BUILD_SLICE";
        case 133: return "JUMP_BACKWARD_NO_INTERRUPT";
        case 134: return "MAKE_CELL";
        case 135: return "LOAD_CLOSURE";
        case 136: return "LOAD_DEREF";
        case 137: return "STORE_DEREF";
        case 138: return "DELETE_DEREF";
        case 139: return "JUMP_BACKWARD";
        case 140: return "CALL_FUNCTION_EX";
        case 141: return "EXTENDED_ARG";
        case 142: return "LIST_APPEND";
        case 143: return "SET_ADD";
        case 144: return "MAP_ADD";
        case 145: return "LOAD_CLASSDEREF";
        case 146: return "COPY_FREE_VARS";
        case 147: return "RESUME";
        case 148: return "MATCH_CLASS";
        case 149: return "FORMAT_VALUE";
        case 150: return "BUILD_CONST_KEY_MAP";
        case 151: return "BUILD_STRING";
        case 152: return "LOAD_METHOD";
        case 153: return "LIST_EXTEND";
        case 154: return "SET_UPDATE";
        case 155: return "DICT_MERGE";
        case 156: return "DICT_UPDATE";
        case 157: return "PRECALL";
        case 158: return "CALL";
        case 159: return "KW_NAMES";
        case 160: return "POP_JUMP_BACKWARD_IF_NOT_NONE";
        case 161: return "POP_JUMP_BACKWARD_IF_NONE";
        case 162: return "POP_JUMP_BACKWARD_IF_FALSE";
        case 163: return "POP_JUMP_BACKWARD_IF_TRUE";
        case 164: return "STORE_ATTR_ADAPTIVE";
        case 165: return "STORE_ATTR_INSTANCE_VALUE";
        case 166: return "STORE_ATTR_SLOT";
        case 167: return "STORE_ATTR_WITH_HINT";
        case 168: return "STORE_FAST__LOAD_FAST";
        case 169: return "STORE_FAST__STORE_FAST";
        case 170: return "STORE_SUBSCR_ADAPTIVE";
        case 171: return "STORE_SUBSCR_DICT";
        case 172: return "STORE_SUBSCR_LIST_INT";
        case 173: return "UNPACK_SEQUENCE_ADAPTIVE";
        case 174: return "UNPACK_SEQUENCE_LIST";
        case 175: return "UNPACK_SEQUENCE_TUPLE";
        case 176: return "UNPACK_SEQUENCE_TWO_TUPLE";
        case 177: return "DO_TRACING";
        default: return "UNKNOWN OPCODE";
    }
}