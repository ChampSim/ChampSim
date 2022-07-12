#include <iostream>
#include <vector>

#include "ooo_cpu.h"
#include "cache.h"
#include "dram_controller.h"

namespace champsim {
  struct phase_stats
  {
    std::vector<O3_CPU::stats_type> roi_cpu_stats, sim_cpu_stats;
    std::vector<CACHE::stats_type> roi_cache_stats, sim_cache_stats;
    std::vector<DRAM_CHANNEL::stats_type> roi_dram_stats, sim_dram_stats;
  };

class plain_printer
{
  std::ostream& stream;

  void print(O3_CPU::stats_type);
  void print(CACHE::stats_type);
  void print(DRAM_CHANNEL::stats_type);

  template <typename T>
  void print(std::vector<T> stats_list)
  {
    for (auto &stats : stats_list)
      print(stats);
  }

  public:
  plain_printer(std::ostream& str) : stream(str) {}
  void print(phase_stats& stats);
};

class json_printer
{
  std::ostream& stream;

  void print(O3_CPU::stats_type);
  void print(CACHE::stats_type);
  void print(DRAM_CHANNEL::stats_type);

  std::size_t indent_level = 0;
  std::string indent() const
  {
    return std::string( 2*indent_level, ' ' );
  }

  void print(std::vector<O3_CPU::stats_type> stats_list);
  void print(std::vector<CACHE::stats_type> stats_list);
  void print(std::vector<DRAM_CHANNEL::stats_type> stats_list);

  public:
  json_printer(std::ostream& str) : stream(str) {}
  void print(phase_stats& stats);
};
}

