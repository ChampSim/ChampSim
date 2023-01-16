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
#include <numeric>
#include <string.h>
#include <vector>

#include <fmt/chrono.h>
#include <fmt/core.h>

#include "ooo_cpu.h"
#include "operable.h"
#include "phase_info.h"
#include "tracereader.h"

auto start_time = std::chrono::steady_clock::now();

std::chrono::seconds elapsed_time()
{
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time);
}

struct phase_info {
  std::string name;
  bool is_warmup;
  uint64_t length;
};

int champsim_main(std::vector<std::reference_wrapper<O3_CPU>>& ooo_cpu, std::vector<std::reference_wrapper<champsim::operable>>& operables,
                  std::vector<champsim::phase_info>& phases, bool knob_cloudsuite, std::vector<std::string> trace_names)
{
  for (champsim::operable& op : operables)
    op.initialize();

  std::vector<std::unique_ptr<tracereader>> traces;
  for (auto name : trace_names)
    traces.push_back(get_tracereader(name, traces.size(), knob_cloudsuite));

  // simulation entry point
  for (auto [phase_name, is_warmup, length, ignored] : phases) {
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
          for (champsim::operable& c : operables) {
            c.print_deadlock();
            fmt::print("\n");
          }

          abort();
        }
      }
      std::sort(std::begin(operables), std::end(operables), champsim::by_next_operate());

      // Read from trace
      for (O3_CPU& cpu : ooo_cpu) {
        auto num_instrs = cpu.IN_QUEUE_SIZE - std::size(cpu.input_queue);
        std::vector<typename decltype(cpu.input_queue)::value_type> from_trace{};

        for (std::size_t i = 0; i < num_instrs; ++i) {
          from_trace.push_back((*traces[cpu.cpu])());

          // Reopen trace if we've reached the end of the file
          if (traces[cpu.cpu]->eof()) {
            auto name = traces[cpu.cpu]->trace_string;
            fmt::print("*** Reached end of trace: {}\n", name);
            traces[cpu.cpu] = get_tracereader(name, cpu.cpu, knob_cloudsuite);
          }
        }

        cpu.input_queue.insert(std::cend(cpu.input_queue), std::begin(from_trace), std::end(from_trace));
      }

      // Check for phase finish
      for (O3_CPU& cpu : ooo_cpu) {
        // Phase complete
        if (!phase_complete[cpu.cpu] && (cpu.sim_instr() >= length)) {
          phase_complete[cpu.cpu] = true;
          for (champsim::operable& op : operables)
            op.end_phase(cpu.cpu);

          fmt::print("{} finished CPU {} instructions: {} cycles: {} cumulative IPC: {:.4g} (Simulation time: {:%H hr %M min %S sec})\n",
              phase_name, cpu.cpu, cpu.sim_instr(), cpu.sim_cycle(), std::ceil(cpu.sim_instr()) / std::ceil(cpu.sim_cycle()), elapsed_time());
        }
      }
    }

    for (O3_CPU& cpu : ooo_cpu) {
      fmt::print("{} complete CPU {} instructions: {} cycles: {} cumulative IPC: {:.4g} (Simulation time: {:%H hr %M min %S sec})\n",
          phase_name, cpu.cpu, cpu.sim_instr(), cpu.sim_cycle(), std::ceil(cpu.sim_instr()) / std::ceil(cpu.sim_cycle()), elapsed_time());
    }
  }

  return 0;
}
