#include <iomanip>
#include <iostream>
#include <numeric>
#include <utility>
#include <vector>

#include "stats_printer.h"

void champsim::plain_printer::print(O3_CPU::stats_type stats)
{
  constexpr std::array<std::pair<std::string_view, std::size_t>, 6> types{
      {std::pair{"BRANCH_DIRECT_JUMP", BRANCH_DIRECT_JUMP}, std::pair{"BRANCH_INDIRECT", BRANCH_INDIRECT}, std::pair{"BRANCH_CONDITIONAL", BRANCH_CONDITIONAL},
       std::pair{"BRANCH_DIRECT_CALL", BRANCH_DIRECT_CALL}, std::pair{"BRANCH_INDIRECT_CALL", BRANCH_INDIRECT_CALL},
       std::pair{"BRANCH_RETURN", BRANCH_RETURN}}};

  uint64_t total_branch = 0, total_mispredictions = 0;
  for (auto type : types) {
    total_branch += stats.total_branch_types[type.second];
    total_mispredictions += stats.branch_type_misses[type.second];
  }

  stream << std::endl;
  stream << stats.name << " cumulative IPC: " << 1.0 * stats.instrs() / stats.cycles() << " instructions: " << stats.instrs() << " cycles: " << stats.cycles()
         << std::endl;
  stream << stats.name << " Branch Prediction Accuracy: " << (100.0 * (total_branch - total_mispredictions)) / total_branch
         << "% MPKI: " << (1000.0 * total_mispredictions) / stats.instrs();
  stream << " Average ROB Occupancy at Mispredict: " << (1.0 * stats.total_rob_occupancy_at_branch_mispredict) / total_mispredictions << std::endl;

  std::vector<double> mpkis;
  std::transform(std::begin(stats.branch_type_misses), std::end(stats.branch_type_misses), std::back_inserter(mpkis),
                 [instrs = stats.instrs()](auto x) { return 1000.0 * x / instrs; });

  stream << "Branch type MPKI" << std::endl;
  for (auto [str, idx] : types)
    stream << str << ": " << mpkis[idx] << std::endl;
  stream << std::endl;
}

void champsim::plain_printer::print(CACHE::stats_type stats)
{
  constexpr std::array<std::pair<std::string_view, std::size_t>, 5> types{
      {std::pair{"LOAD", LOAD}, std::pair{"RFO", RFO}, std::pair{"PREFETCH", PREFETCH}, std::pair{"WRITE", WRITE}, std::pair{"TRANSLATION", TRANSLATION}}};

  for (std::size_t cpu = 0; cpu < NUM_CPUS; ++cpu) {
    uint64_t TOTAL_HIT = 0, TOTAL_MISS = 0;
    for (const auto& type : types) {
      TOTAL_HIT += stats.hits.at(type.second).at(cpu);
      TOTAL_MISS += stats.misses.at(type.second).at(cpu);
    }

    stream << stats.name << " TOTAL       ";
    stream << "ACCESS: " << std::setw(10) << TOTAL_HIT + TOTAL_MISS << "  ";
    stream << "HIT: " << std::setw(10) << TOTAL_HIT << "  ";
    stream << "MISS: " << std::setw(10) << TOTAL_MISS << std::endl;

    for (const auto& type : types) {
      stream << stats.name << " " << type.first << std::setw(12 - std::size(type.first)) << " ";
      stream << "ACCESS: " << std::setw(10) << stats.hits[type.second][cpu] + stats.misses[type.second][cpu] << "  ";
      stream << "HIT: " << std::setw(10) << stats.hits[type.second][cpu] << "  ";
      stream << "MISS: " << std::setw(10) << stats.misses[type.second][cpu] << std::endl;
    }

    stream << stats.name << " PREFETCH  ";
    stream << "REQUESTED: " << std::setw(10) << stats.pf_requested << "  ";
    stream << "ISSUED: " << std::setw(10) << stats.pf_issued << "  ";
    stream << "USEFUL: " << std::setw(10) << stats.pf_useful << "  ";
    stream << "USELESS: " << std::setw(10) << stats.pf_useless << std::endl;

    stream << stats.name << " AVERAGE MISS LATENCY: " << (1.0 * (stats.total_miss_latency)) / TOTAL_MISS << " cycles" << std::endl;
    // stream << " AVERAGE MISS LATENCY: " << (stats.total_miss_latency)/TOTAL_MISS << " cycles " << stats.total_miss_latency << "/" << TOTAL_MISS<< std::endl;
  }
}

void champsim::plain_printer::print(DRAM_CHANNEL::stats_type stats)
{
  stream << stats.name << std::endl;
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

void champsim::plain_printer::print(champsim::phase_stats& stats)
{
  stream << "=== " << stats.name << " ===" << std::endl;

  int i = 0;
  for (auto tn : stats.trace_names)
    stream << "CPU " << i++ << " runs " << tn << std::endl;

  if (NUM_CPUS > 1) {
    stream << std::endl;
    stream << "Total Simulation Statistics (not including warmup)" << std::endl;

    for (const auto& stat : stats.sim_cpu_stats)
      print(stat);

    for (const auto& stat : stats.sim_cache_stats)
      print(stat);
  }

  stream << std::endl;
  stream << "Region of Interest Statistics" << std::endl;

  for (const auto& stat : stats.roi_cpu_stats)
    print(stat);

  for (const auto& stat : stats.roi_cache_stats)
    print(stat);

  stream << std::endl;
  stream << "DRAM Statistics" << std::endl;
  for (const auto& stat : stats.roi_dram_stats)
    print(stat);
}

void champsim::plain_printer::print(std::vector<phase_stats>& stats)
{
  for (auto p : stats)
    print(p);
}
