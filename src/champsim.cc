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

#include "ooo_cpu.h"
#include "operable.h"
#include "tracereader.h"

auto start_time = std::chrono::steady_clock::now();

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

int champsim_main(std::vector<std::reference_wrapper<O3_CPU>>& ooo_cpu, std::vector<std::reference_wrapper<champsim::operable>>& operables,
                  std::vector<phase_info>& phases, bool knob_cloudsuite, std::vector<std::string> trace_names)
{
  for (champsim::operable& op : operables)
    op.initialize();

  std::vector<std::unique_ptr<tracereader>> traces;
  for (auto name : trace_names)
    traces.push_back(get_tracereader(name, traces.size(), knob_cloudsuite));

  // simulation entry point
  for (auto [phase_name, is_warmup, length] : phases) {
    // Initialize phase
    for (champsim::operable& op : operables) {
      op.warmup = is_warmup;
      op.begin_phase();
    }

    // Perform phase
    std::vector<bool> phase_complete(std::size(ooo_cpu), false);
    while (!std::accumulate(std::begin(phase_complete), std::end(phase_complete), true, std::logical_and{})) {
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
          cpu.input_queue.push_back((*traces[cpu.cpu])());

          // Reopen trace if we've reached the end of the file
          if (traces[cpu.cpu]->eof()) {
            auto name = traces[cpu.cpu]->trace_string;
            std::cout << "*** Reached end of trace: " << name << std::endl;
            traces[cpu.cpu] = get_tracereader(name, cpu.cpu, knob_cloudsuite);
          }
        }
      }

      // Check for phase finish
      auto [elapsed_hour, elapsed_minute, elapsed_second] = elapsed_time();
      for (O3_CPU& cpu : ooo_cpu) {
        // Phase complete
        if (!phase_complete[cpu.cpu] && (cpu.sim_instr() >= length)) {
          phase_complete[cpu.cpu] = true;
          for (champsim::operable& op : operables)
            op.end_phase(cpu.cpu);

          std::cout << phase_name << " finished CPU " << cpu.cpu;
          std::cout << " instructions: " << cpu.sim_instr() << " cycles: " << cpu.sim_cycle() << " cumulative IPC: " << 1.0 * cpu.sim_instr() / cpu.sim_cycle();
          std::cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << std::endl;
        }
      }
    }

    auto [elapsed_hour, elapsed_minute, elapsed_second] = elapsed_time();
    for (O3_CPU& cpu : ooo_cpu) {
      std::cout << std::endl;
      std::cout << phase_name << " complete CPU " << cpu.cpu << " instructions: " << cpu.sim_instr() << " cycles: " << cpu.sim_cycle();
      std::cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << std::endl;
      std::cout << std::endl;
    }
  }

  return 0;
}
