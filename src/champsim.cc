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

#include "champsim.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <getopt.h>
#include <iomanip>
#include <numeric>
#include <string.h>
#include <vector>

#include "ooo_cpu.h"
#include "operable.h"
#include "phase_info.h"
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

namespace champsim
{
void do_phase(phase_info phase, std::vector<std::reference_wrapper<O3_CPU>>& ooo_cpu, std::vector<std::reference_wrapper<operable>>& operables, std::vector<tracereader>& traces)
{
  auto [phase_name, is_warmup, length, trace_names] = phase;

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
    for (O3_CPU& cpu : ooo_cpu)
      std::generate_n(std::back_inserter(cpu.input_queue), cpu.IN_QUEUE_SIZE - std::size(cpu.input_queue), std::ref(traces[cpu.cpu]));

    // Check for phase finish
    auto [elapsed_hour, elapsed_minute, elapsed_second] = elapsed_time();
    for (O3_CPU& cpu : ooo_cpu) {
      // Phase complete
      if (!phase_complete[cpu.cpu] && (cpu.sim_instr() >= length)) {
        phase_complete[cpu.cpu] = true;
        for (champsim::operable& op : operables)
          op.end_phase(cpu.cpu);

        std::cout << phase_name << " finished CPU " << cpu.cpu;
        std::cout << " instructions: " << cpu.sim_instr() << " cycles: " << cpu.sim_cycle()
                  << " cumulative IPC: " << std::ceil(cpu.sim_instr()) / std::ceil(cpu.sim_cycle());
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

// simulation entry point
int main(std::vector<std::reference_wrapper<O3_CPU>>& ooo_cpu, std::vector<std::reference_wrapper<operable>>& operables, std::vector<phase_info>& phases, bool knob_cloudsuite, std::vector<std::string> trace_names)
{
  for (champsim::operable& op : operables)
    op.initialize();

  std::vector<champsim::tracereader> traces;
  for (auto name : trace_names)
    traces.push_back(get_tracereader(name, traces.size(), knob_cloudsuite));

  for (auto phase : phases)
    do_phase(phase, ooo_cpu, operables, traces);

  return 0;
}
}
