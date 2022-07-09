#include <iostream>
#include <iomanip>
#include <numeric>
#include <utility>
#include <vector>

#include "ooo_cpu.h"
#include "cache.h"
#include "dram_controller.h"

template <typename Ostr>
void print_cpu_stats(Ostr& stream, std::string_view name, O3_CPU::stats_type stats, uint64_t instrs, uint64_t cycles)
{
  constexpr std::array<std::pair<std::string_view, std::size_t>, 6> types{{
    std::pair{"BRANCH_DIRECT_JUMP", BRANCH_DIRECT_JUMP},
    std::pair{"BRANCH_INDIRECT", BRANCH_INDIRECT},
    std::pair{"BRANCH_CONDITIONAL", BRANCH_CONDITIONAL},
    std::pair{"BRANCH_DIRECT_CALL", BRANCH_DIRECT_CALL},
    std::pair{"BRANCH_INDIRECT_CALL", BRANCH_INDIRECT_CALL},
    std::pair{"BRANCH_RETURN", BRANCH_RETURN}
  }};

  uint64_t total_branch = 0, total_mispredictions = 0;
  for (auto type : types) {
    total_branch += stats.total_branch_types[type.second];
    total_mispredictions += stats.branch_type_misses[type.second];
  }

  stream << std::endl;
  stream << name << " cumulative IPC: " << 1.0 * instrs / cycles << " instructions: " << instrs << " cycles: " << cycles << std::endl;
  stream << name << " Branch Prediction Accuracy: " << (100.0 * (total_branch - total_mispredictions)) / total_branch << "% MPKI: " << (1000.0 * total_mispredictions) / instrs;
  stream << " Average ROB Occupancy at Mispredict: " << (1.0 * stats.total_rob_occupancy_at_branch_mispredict) / total_mispredictions << std::endl;

  std::vector<double> mpkis;
  std::transform(std::begin(stats.branch_type_misses), std::end(stats.branch_type_misses), std::back_inserter(mpkis), [instrs](auto x) { return 1000.0 * x / instrs; });

  stream << "Branch type MPKI" << std::endl;
  for (auto [str, idx] : types)
    stream << str << ": " << mpkis[idx] << std::endl;
  stream << std::endl;
}

template <typename Ostr>
void json_print_cpu_stats(Ostr& stream, std::string_view, O3_CPU::stats_type stats, uint64_t instrs, uint64_t cycles)
{
  constexpr std::array<std::pair<std::string_view, std::size_t>, 6> types{{
    std::pair{"BRANCH_DIRECT_JUMP", BRANCH_DIRECT_JUMP},
    std::pair{"BRANCH_INDIRECT", BRANCH_INDIRECT},
    std::pair{"BRANCH_CONDITIONAL", BRANCH_CONDITIONAL},
    std::pair{"BRANCH_DIRECT_CALL", BRANCH_DIRECT_CALL},
    std::pair{"BRANCH_INDIRECT_CALL", BRANCH_INDIRECT_CALL},
    std::pair{"BRANCH_RETURN", BRANCH_RETURN}
  }};

  uint64_t total_branch = 0, total_mispredictions = 0;
  for (auto type : types) {
    total_branch += stats.total_branch_types[type.second];
    total_mispredictions += stats.branch_type_misses[type.second];
  }

  stream << "{" << std::endl;
  stream << "  \"instructions\": " << instrs << "," << std::endl;
  stream << "  \"cycles\": " << cycles << "," << std::endl;
  stream << "  \"Avg ROB occupancy at mispredict\": " << (1.0 * stats.total_rob_occupancy_at_branch_mispredict) / total_mispredictions << ", " << std::endl;

  stream << "  \"mispredict\": {" << std::endl;
  for (std::size_t i = 0; i < std::size(types); ++i) {
    if (i != 0)
      stream << ", " << std::endl;
    stream << "    \"" << types[i].first << "\": " << stats.branch_type_misses[types[i].second];
  }
  stream << std::endl;
  stream << "  }" << std::endl;
  stream << "}" << std::endl;
}

template<typename Ostr>
void print_cache_stats(Ostr& stream, std::string name, CACHE::stats_type stats)
{
  constexpr std::array<std::pair<std::string_view, std::size_t>, 5> types{{std::pair{"LOAD", LOAD}, std::pair{"RFO", RFO}, std::pair{"PREFETCH", PREFETCH}, std::pair{"WRITE", WRITE}, std::pair{"TRANSLATION", TRANSLATION}}};

  for (std::size_t cpu = 0; cpu < NUM_CPUS; ++cpu) {
    uint64_t TOTAL_HIT = 0, TOTAL_MISS = 0;
    for (const auto& type : types) {
      TOTAL_HIT += stats.hits.at(type.second).at(cpu);
      TOTAL_MISS += stats.misses.at(type.second).at(cpu);
    }

    stream << name << " TOTAL       ";
    stream << "ACCESS: " << std::setw(10) << TOTAL_HIT + TOTAL_MISS << "  ";
    stream << "HIT: " << std::setw(10) << TOTAL_HIT << "  ";
    stream << "MISS: " << std::setw(10) << TOTAL_MISS << std::endl;

    for (const auto& type : types) {
      stream << name << " " << type.first << std::setw(12-std::size(type.first)) << " ";
      stream << "ACCESS: " << std::setw(10) << stats.hits[type.second][cpu] + stats.misses[type.second][cpu] << "  ";
      stream << "HIT: " << std::setw(10) << stats.hits[type.second][cpu] << "  ";
      stream << "MISS: " << std::setw(10) << stats.misses[type.second][cpu] << std::endl;
    }

    stream << name << " PREFETCH  ";
    stream << "REQUESTED: " << std::setw(10) << stats.pf_requested << "  ";
    stream << "ISSUED: " << std::setw(10) << stats.pf_issued << "  ";
    stream << "USEFUL: " << std::setw(10) << stats.pf_useful << "  ";
    stream << "USELESS: " << std::setw(10) << stats.pf_useless << std::endl;

    stream << name << " AVERAGE MISS LATENCY: " << (1.0 * (stats.total_miss_latency)) / TOTAL_MISS << " cycles" << std::endl;
    // stream << " AVERAGE MISS LATENCY: " << (stats.total_miss_latency)/TOTAL_MISS << " cycles " << stats.total_miss_latency << "/" << TOTAL_MISS<< std::endl;
  }
}

template<typename Ostr>
void json_print_cache_stats(Ostr& stream, std::string, CACHE::stats_type stats)
{
  constexpr std::array<std::pair<std::string_view, std::size_t>, 5> types{{std::pair{"LOAD", LOAD}, std::pair{"RFO", RFO}, std::pair{"PREFETCH", PREFETCH}, std::pair{"WRITE", WRITE}, std::pair{"TRANSLATION", TRANSLATION}}};

  stream << "{" << std::endl;
  for (const auto& type : types) {
    stream << "  \"" << type.first << "\": {" << std::endl;

    stream << "    \"hit\": [";
    for (std::size_t i = 0; i < std::size(stats.hits[type.second]); ++i) {
      if (i != 0)
        stream << ", ";
      stream << stats.hits[type.second][i];
    }
    stream << "]," << std::endl;

    stream << "    \"miss\": [";
    for (std::size_t i = 0; i < std::size(stats.misses[type.second]); ++i) {
      if (i != 0)
        stream << ", ";
      stream << stats.misses[type.second][i];
    }
    stream << "]" << std::endl;

    stream << "  }," << std::endl;
  }

  stream << "  \"prefetch requested\": " << stats.pf_requested << "," << std::endl;
  stream << "  \"prefetch issued\": " << stats.pf_issued << "," << std::endl;
  stream << "  \"useful prefetch\": " << stats.pf_useful << "," << std::endl;
  stream << "  \"useless prefetch\": " << stats.pf_useless << "," << std::endl;

  uint64_t TOTAL_MISS = 0;
  for (const auto& type : types)
    TOTAL_MISS += std::accumulate(std::begin(stats.misses.at(type.second)), std::end(stats.misses.at(type.second)), TOTAL_MISS);
  stream << "  \"miss latency\": " << (1.0 * (stats.total_miss_latency)) / TOTAL_MISS << std::endl;
  stream << "}" << std::endl;
}

void CACHE::print_roi_stats()
{
  json_print_cache_stats(std::cout, NAME, roi_stats.back());
}

void CACHE::print_phase_stats()
{
  json_print_cache_stats(std::cout, NAME, sim_stats.back());
}

void O3_CPU::print_roi_stats()
{
  json_print_cpu_stats(std::cout, "CPU" + std::to_string(cpu), roi_stats.back(), roi_instr(), roi_cycle());
}

void O3_CPU::print_phase_stats()
{
  json_print_cpu_stats(std::cout, "CPU" + std::to_string(cpu), sim_stats.back(), sim_instr(), sim_cycle());
}

template <typename Ostr>
void print_dram_channel_stats(Ostr& stream, std::string_view name, DRAM_CHANNEL::stats_type stats)
{
  stream << name << std::endl;
  stream << " RQ ROW_BUFFER_HIT: " << std::setw(10) << stats.RQ_ROW_BUFFER_HIT << std::endl;
  stream << "  ROW_BUFFER_MISS: " << std::setw(10) << stats.RQ_ROW_BUFFER_MISS << std::endl;
  stream << " AVG DBUS CONGESTED CYCLE: ";
  if (stats.dbus_count_congested > 0)
    stream << std::setw(10) << (1.0 * stats.dbus_cycle_congested) / stats.dbus_count_congested;
  else
    stream << "-";
  stream << std::endl;
  stream << " WQ ROW_BUFFER_HIT: " << std::setw(10) << stats.WQ_ROW_BUFFER_HIT << std::endl;
  stream << "  ROW_BUFFER_MISS: " << std::setw(10) << stats.WQ_ROW_BUFFER_MISS;
  stream << "  FULL: " << std::setw(10) << stats.WQ_FULL << std::endl;
  stream << std::endl;
}

template <typename Ostr>
void json_print_dram_channel_stats(Ostr& stream, std::string_view, DRAM_CHANNEL::stats_type stats)
{
  stream << "{" << std::endl;
  stream << "  \"RQ ROW_BUFFER_HIT\": " << stats.RQ_ROW_BUFFER_HIT << "," << std::endl;
  stream << "  \"RQ ROW_BUFFER_MISS\": " << stats.RQ_ROW_BUFFER_MISS << "," << std::endl;
  stream << "  \"WQ ROW_BUFFER_HIT\": " << stats.WQ_ROW_BUFFER_HIT << "," << std::endl;
  stream << "  \"WQ ROW_BUFFER_MISS\": " << stats.WQ_ROW_BUFFER_MISS << "," << std::endl;
  stream << "  \"WQ FULL\": " << stats.WQ_FULL << "," << std::endl;
  if (stats.dbus_count_congested > 0)
    stream << "  \"AVG DBUS CONGESTED CYCLE\": " << (1.0 * stats.dbus_cycle_congested) / stats.dbus_count_congested << std::endl;
  else
    stream << "  \"AVG DBUS CONGESTED CYCLE\": null" << std::endl;
  stream << "}";
}

void MEMORY_CONTROLLER::print_roi_stats() {}

void MEMORY_CONTROLLER::print_phase_stats()
{
  std::size_t chan_idx = 0;
  std::cout << "\"DRAM\": [" << std::endl;
  for (const auto &chan : channels) {
    if (chan_idx != 0)
      std::cout << "," << std::endl;
    json_print_dram_channel_stats(std::cout, "DRAM CHANNEL " + std::to_string(chan_idx++), chan.sim_stats.back());
  }
  std::cout << std::endl;
  std::cout << "]" << std::endl;
}

