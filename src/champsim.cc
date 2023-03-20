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
#include <chrono>
#include <numeric>
#include <vector>

#include "environment.h"
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
phase_stats do_phase(phase_info phase, environment& env, std::vector<tracereader>& traces)
{
  auto [phase_name, is_warmup, length, trace_names] = phase;
  auto operables = env.operable_view();

  // Initialize phase
  for (champsim::operable& op : operables) {
    op.warmup = is_warmup;
    op.begin_phase();
  }

  // Perform phase
  std::vector<bool> phase_complete(std::size(env.cpu_view()), false);
  while (!std::accumulate(std::begin(phase_complete), std::end(phase_complete), true, std::logical_and{})) {
    // Operate
    for (champsim::operable& op : operables) {
      try {
        op._operate();
      } catch (champsim::deadlock& dl) {
        // env.cpu_view()[dl.which].print_deadlock();
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
    for (O3_CPU& cpu : env.cpu_view())
      std::generate_n(std::back_inserter(cpu.input_queue), cpu.IN_QUEUE_SIZE - std::size(cpu.input_queue), std::ref(traces[cpu.cpu]));

    // Check for phase finish
    auto [elapsed_hour, elapsed_minute, elapsed_second] = elapsed_time();
    for (O3_CPU& cpu : env.cpu_view()) {
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
  for (O3_CPU& cpu : env.cpu_view()) {
    std::cout << std::endl;
    std::cout << phase_name << " complete CPU " << cpu.cpu << " instructions: " << cpu.sim_instr() << " cycles: " << cpu.sim_cycle();
    std::cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << std::endl;
    std::cout << std::endl;
  }

  phase_stats stats;
  stats.name = phase.name;
  stats.trace_names = phase.trace_names;

  auto cpus = env.cpu_view();
  std::transform(std::begin(cpus), std::end(cpus), std::back_inserter(stats.sim_cpu_stats), [](const O3_CPU& cpu) { return cpu.sim_stats; });
  std::transform(std::begin(cpus), std::end(cpus), std::back_inserter(stats.roi_cpu_stats), [](const O3_CPU& cpu) { return cpu.roi_stats; });

  auto caches = env.cache_view();
  std::transform(std::begin(caches), std::end(caches), std::back_inserter(stats.sim_cache_stats),
                 [](const CACHE& cache) { return cache.sim_stats; });
  std::transform(std::begin(caches), std::end(caches), std::back_inserter(stats.roi_cache_stats),
                 [](const CACHE& cache) { return cache.roi_stats; });

  auto dram = env.dram_view();
  std::transform(std::begin(dram.channels), std::end(dram.channels), std::back_inserter(stats.sim_dram_stats),
                 [](const DRAM_CHANNEL& chan) { return chan.sim_stats; });
  std::transform(std::begin(dram.channels), std::end(dram.channels), std::back_inserter(stats.roi_dram_stats),
                 [](const DRAM_CHANNEL& chan) { return chan.roi_stats; });

  return stats;
}

// simulation entry point
std::vector<phase_stats> main(environment& env, std::vector<phase_info>& phases, bool knob_cloudsuite, std::vector<std::string> trace_names)
{
  for (champsim::operable& op : env.operable_view())
    op.initialize();

  std::vector<champsim::tracereader> traces;
  for (auto name : trace_names)
    traces.push_back(get_tracereader(name, traces.size(), knob_cloudsuite));

  std::vector<phase_stats> results;
  for (auto phase : phases) {
    auto stats = do_phase(phase, env, traces);
    if (!phase.is_warmup)
      results.push_back(stats);
  }

  return results;
}
} // namespace champsim
