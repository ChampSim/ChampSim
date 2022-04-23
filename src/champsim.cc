#include "champsim.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <fstream>
#include <functional>
#include <getopt.h>
#include <iomanip>
#include <numeric>
#include <string.h>
#include <vector>

#include "cache.h"
#include "champsim_constants.h"
#include "dram_controller.h"
#include "ooo_cpu.h"
#include "operable.h"
#include "tracereader.h"
#include "vmem.h"

auto start_time = std::chrono::steady_clock::now();

// For backwards compatibility with older module source.
champsim::deprecated_clock_cycle current_core_cycle;

extern MEMORY_CONTROLLER DRAM;
extern VirtualMemory vmem;
extern std::array<std::reference_wrapper<O3_CPU>, NUM_CPUS> ooo_cpu;
extern std::array<std::reference_wrapper<CACHE>, NUM_CACHES> caches;
extern std::array<std::reference_wrapper<champsim::operable>, 2*std::size(ooo_cpu) + 2*std::size(caches) + 1> operables;

void init_structures();

uint64_t champsim::deprecated_clock_cycle::operator[](std::size_t cpu_idx)
{
  static bool deprecate_printed = false;
  if (!deprecate_printed) {
    std::cout << "WARNING: The use of 'current_core_cycle[cpu]' is deprecated." << std::endl;
    std::cout << "WARNING: Use 'this->current_cycle' instead." << std::endl;
    deprecate_printed = true;
  }
  return ooo_cpu[cpu_idx].get().current_cycle;
}

std::tuple<uint64_t, uint64_t, uint64_t> elapsed_time()
{
  auto diff = std::chrono::steady_clock::now() - start_time;
  auto elapsed_hour = std::chrono::duration_cast<std::chrono::hours>(diff);
  auto elapsed_minute = std::chrono::duration_cast<std::chrono::minutes>(diff) - elapsed_hour;
  auto elapsed_second = std::chrono::duration_cast<std::chrono::seconds>(diff) - elapsed_hour - elapsed_minute;
  return {elapsed_hour.count(), elapsed_minute.count(), elapsed_second.count()};
}

struct phase_info {
  std::string name;
  bool is_warmup;
  uint64_t length;
};

int champsim_main(uint64_t warmup_instructions, uint64_t simulation_instructions, bool show_heartbeat, bool knob_cloudsuite,
                  std::vector<std::string> trace_names)
{
  std::cout << std::endl << "*** ChampSim Multicore Out-of-Order Simulator ***" << std::endl << std::endl;

  // consequences of knobs
  std::cout << "Warmup Instructions: " << warmup_instructions << std::endl;
  std::cout << "Simulation Instructions: " << simulation_instructions << std::endl;
  std::cout << "Number of CPUs: " << NUM_CPUS << std::endl;

  long long int dram_size = DRAM_CHANNELS * DRAM_RANKS * DRAM_BANKS * DRAM_ROWS * DRAM_COLUMNS * BLOCK_SIZE / 1024 / 1024; // in MiB
  std::cout << "Off-chip DRAM Size: ";
  if (dram_size > 1024)
    std::cout << dram_size / 1024 << " GiB";
  else
    std::cout << dram_size << " MiB";
  std::cout << " Channels: " << DRAM_CHANNELS << " Width: " << 8 * DRAM_CHANNEL_WIDTH << "-bit Data Rate: " << DRAM_IO_FREQ << " MT/s" << std::endl;

  std::cout << std::endl;
  std::cout << "VirtualMemory physical capacity: " << std::size(vmem.ppage_free_list) * vmem.page_size;
  std::cout << " num_ppages: " << std::size(vmem.ppage_free_list) << std::endl;
  std::cout << "VirtualMemory page size: " << PAGE_SIZE << " log2_page_size: " << LOG2_PAGE_SIZE << std::endl;

  std::cout << std::endl;
  std::vector<supported_tracereader> traces;
  for (auto name : trace_names) {
    std::cout << "CPU " << traces.size() << " runs " << name << std::endl;

    traces.push_back(get_tracereader(name, traces.size(), knob_cloudsuite));

    if (traces.size() > NUM_CPUS) {
      printf("\n*** Too many traces for the configured number of cores ***\n\n");
      return 1;
    }
  }

  if (traces.size() != NUM_CPUS) {
    printf("\n*** Not enough traces for the configured number of cores ***\n\n");
    return 1;
  }
  // end trace file setup

  init_structures();

  for (O3_CPU& cpu : ooo_cpu) {
    cpu.initialize_core();
  }

  for (CACHE& cache : caches) {
    cache.impl_prefetcher_initialize();
    cache.impl_replacement_initialize();
  }

  std::vector<phase_info> phases{{phase_info{"Warmup", true, warmup_instructions}, phase_info{"Simulation", false, simulation_instructions}}};

  // simulation entry point
  for (auto phase : phases) {
    // Initialize phase
    for (champsim::operable& op : operables) {
      op.warmup = phase.is_warmup;
      op.begin_phase();
    }

    // Perform phase
    std::bitset<NUM_CPUS> phase_complete = {};
    while (!phase_complete.all()) {
      // Operate
      for (champsim::operable& op : operables) {
        try {
          op._operate();
        } catch (champsim::deadlock& dl) {
          // ooo_cpu[dl.which].print_deadlock();
          // std::cout << std::endl;
          // for (auto c : caches)
          for (champsim::operable& c : operables) {
            c.print_deadlock();
            std::cout << std::endl;
          }

          abort();
        }
      }
      std::sort(std::begin(operables), std::end(operables), champsim::by_next_operate());

      // Read from trace
      for (O3_CPU& cpu : ooo_cpu) {
        while (std::size(cpu.input_queue) < cpu.IN_QUEUE_SIZE) {
          cpu.input_queue.push_back(std::visit(get_instr{}, traces[cpu.cpu]));

          // Reopen trace if we've reached the end of the file
          if (std::visit(get_eof{}, traces[cpu.cpu])) {
            auto name = std::visit(get_trace_string{}, traces[cpu.cpu]);
            std::cout << "*** Reached end of trace: " << name << std::endl;
            traces[cpu.cpu] = get_tracereader(name, cpu.cpu, knob_cloudsuite);
          }
        }
      }

      // Check for phase finish
      auto [elapsed_hour, elapsed_minute, elapsed_second] = elapsed_time();
      for (O3_CPU& cpu : ooo_cpu) {
        // Phase complete
        if (!phase_complete[cpu.cpu] && (cpu.sim_instr() >= phase.length)) {
          phase_complete.set(cpu.cpu);
          for (champsim::operable& op : operables)
            op.end_phase(cpu.cpu);

          std::cout << phase.name << " finished CPU " << cpu.cpu;
          std::cout << " instructions: " << cpu.sim_instr() << " cycles: " << cpu.sim_cycle() << " cumulative IPC: " << 1.0 * cpu.sim_instr() / cpu.sim_cycle();
          std::cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << std::endl;
        }
      }
    }

    auto [elapsed_hour, elapsed_minute, elapsed_second] = elapsed_time();
    for (O3_CPU& cpu : ooo_cpu) {
      std::cout << std::endl;
      std::cout << phase.name << " complete CPU " << cpu.cpu << " instructions: " << cpu.sim_instr() << " cycles: " << cpu.sim_cycle();
      std::cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << std::endl;
      std::cout << std::endl;
    }
  }

  std::cout << "ChampSim completed all CPUs" << std::endl;

  if (NUM_CPUS > 1) {
    std::cout << std::endl;
    std::cout << "Total Simulation Statistics (not including warmup)" << std::endl;

    for (O3_CPU& cpu : ooo_cpu)
      cpu.print_phase_stats();

    for (CACHE& cache : caches)
      cache.print_phase_stats();
  }

  std::cout << std::endl;
  std::cout << "Region of Interest Statistics" << std::endl;
  for (O3_CPU& cpu : ooo_cpu)
    cpu.print_roi_stats();

  for (CACHE& cache : caches)
    cache.print_roi_stats();

  for (CACHE& cache : caches)
    cache.impl_prefetcher_final_stats();

  for (CACHE& cache : caches)
    cache.impl_replacement_final_stats();

  DRAM.print_phase_stats();

  return 0;
}
