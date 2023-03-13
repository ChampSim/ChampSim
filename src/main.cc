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

#include <algorithm>
#include <array>
#include <CLI/CLI.hpp>
#include <fstream>
#include <functional>
#include <iostream>
#include <signal.h>
#include <string>
#include <vector>

#include "cache.h"
#include "champsim.h"
#include "champsim_constants.h"
#include "dram_controller.h"
#include "ooo_cpu.h"
#include "operable.h"
#include "phase_info.h"
#include "ptw.h"
#include "stats_printer.h"
#include "util.h"
#include "vmem.h"

void init_structures();

#include "core_inst.inc"

int champsim_main(std::vector<std::reference_wrapper<O3_CPU>>& cpus, std::vector<std::reference_wrapper<champsim::operable>>& operables,
                  std::vector<champsim::phase_info>& phases, bool knob_cloudsuite, std::vector<std::string> trace_names);

void signal_handler(int signal)
{
  std::cout << "Caught signal: " << signal << std::endl;
  abort();
}

template <typename CPU, typename C, typename D>
std::vector<champsim::phase_stats> zip_phase_stats(const std::vector<champsim::phase_info>& phases, const std::vector<CPU>& cpus,
                                                   const std::vector<C>& cache_list, const D& dram)
{
  std::vector<champsim::phase_stats> retval;

  for (std::size_t i = 0; i < std::size(phases); ++i) {
    if (!phases.at(i).is_warmup) {
      champsim::phase_stats stats;

      stats.name = phases.at(i).name;
      stats.trace_names = phases.at(i).trace_names;

      std::transform(std::begin(cpus), std::end(cpus), std::back_inserter(stats.sim_cpu_stats), [i](const O3_CPU& cpu) { return cpu.sim_stats.at(i); });
      std::transform(std::begin(cache_list), std::end(cache_list), std::back_inserter(stats.sim_cache_stats),
                     [i](const CACHE& cache) { return cache.sim_stats.at(i); });
      std::transform(std::begin(dram.channels), std::end(dram.channels), std::back_inserter(stats.sim_dram_stats),
                     [i](const DRAM_CHANNEL& chan) { return chan.sim_stats.at(i); });
      std::transform(std::begin(cpus), std::end(cpus), std::back_inserter(stats.roi_cpu_stats), [i](const O3_CPU& cpu) { return cpu.roi_stats.at(i); });
      std::transform(std::begin(cache_list), std::end(cache_list), std::back_inserter(stats.roi_cache_stats),
                     [i](const CACHE& cache) { return cache.roi_stats.at(i); });
      std::transform(std::begin(dram.channels), std::end(dram.channels), std::back_inserter(stats.roi_dram_stats),
                     [i](const DRAM_CHANNEL& chan) { return chan.roi_stats.at(i); });

      retval.push_back(stats);
    }
  }

  return retval;
}

int main(int argc, char** argv)
{
  // interrupt signal hanlder
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = signal_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  CLI::App app{"A microarchitecture simulator for research and education"};

  bool knob_cloudsuite{false};
  uint64_t warmup_instructions = 1000000;
  uint64_t simulation_instructions = 10000000;
  std::string json_file_name;
  std::vector<std::string> trace_names;

  auto set_heartbeat_callback = [&](auto){
    for (O3_CPU& cpu : ooo_cpu)
      cpu.show_heartbeat = false;
  };

  app.add_flag("-c,--cloudsuite", knob_cloudsuite, "Read all traces using the cloudsuite format");
  app.add_flag("--hide-heartbeat", set_heartbeat_callback, "Hide the heartbeat output");
  app.add_option("-w,--warmup-instructions", warmup_instructions, "The number of instructions in the warmup phase");
  app.add_option("-i,--simulation-instructions", simulation_instructions, "The number of instructions in the detailed phase");

  auto json_option = app.add_option("--json", json_file_name, "The name of the file to receive JSON output. If no name is specified, stdout will be used")
    ->expected(0,1);

  app.add_option("traces", trace_names, "The paths to the traces")
    ->required()
    ->expected(NUM_CPUS)
    ->check(CLI::ExistingFile);

  CLI11_PARSE(app, argc, argv);

  std::vector<champsim::phase_info> phases{{champsim::phase_info{"Warmup", true, warmup_instructions, trace_names},
                                            champsim::phase_info{"Simulation", false, simulation_instructions, trace_names}}};

  std::cout << std::endl;
  std::cout << "*** ChampSim Multicore Out-of-Order Simulator ***" << std::endl;
  std::cout << std::endl;
  std::cout << "Warmup Instructions: " << phases[0].length << std::endl;
  std::cout << "Simulation Instructions: " << phases[1].length << std::endl;
  std::cout << "Number of CPUs: " << std::size(ooo_cpu) << std::endl;
  std::cout << "Page size: " << PAGE_SIZE << std::endl;
  std::cout << std::endl;

  init_structures();

  champsim_main(ooo_cpu, operables, phases, knob_cloudsuite, trace_names);

  std::cout << std::endl;
  std::cout << "ChampSim completed all CPUs" << std::endl;
  std::cout << std::endl;

  auto phase_stats = zip_phase_stats(phases, ooo_cpu, caches, DRAM);

  champsim::plain_printer{std::cout}.print(phase_stats);

  for (CACHE& cache : caches)
    cache.impl_prefetcher_final_stats();

  for (CACHE& cache : caches)
    cache.impl_replacement_final_stats();

  if (json_option->count() > 0) {
    if (json_file_name.empty()) {
      champsim::json_printer{std::cout}.print(phase_stats);
    } else {
      std::ofstream json_file{json_file_name};
      champsim::json_printer{json_file}.print(phase_stats);
    }
  }

  return 0;
}
