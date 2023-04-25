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
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "champsim.h"
#include "champsim_constants.h"
#include "core_inst.inc"
#include "defaults.hpp"
#include "phase_info.h"
#include "stats_printer.h"
#include "tracereader.h"
#include "util.h"
#include "vmem.h"

namespace champsim
{
std::vector<phase_stats> main(environment& env, std::vector<phase_info>& phases, std::vector<tracereader>& traces);
}

int main(int argc, char** argv)
{
  champsim::configured::generated_environment gen_environment{};

  // initialize knobs
  uint8_t knob_cloudsuite = 0;
  uint64_t warmup_instructions = 1000000, simulation_instructions = 10000000;
  bool knob_json_out = false;
  std::ofstream json_file;

  // check to see if knobs changed using getopt_long()
  int traces_encountered = 0;
  static struct option long_options[] = {{"warmup_instructions", required_argument, 0, 'w'},
                                         {"simulation_instructions", required_argument, 0, 'i'},
                                         {"hide_heartbeat", no_argument, 0, 'h'},
                                         {"cloudsuite", no_argument, 0, 'c'},
                                         {"json", optional_argument, 0, 'j'},
                                         {"traces", no_argument, &traces_encountered, 1},
                                         {0, 0, 0, 0}};

  int c;
  while ((c = getopt_long_only(argc, argv, "w:i:hc", long_options, NULL)) != -1 && !traces_encountered) {
    switch (c) {
    case 'w':
      warmup_instructions = atol(optarg);
      break;
    case 'i':
      simulation_instructions = atol(optarg);
      break;
    case 'h':
      for (O3_CPU& cpu : gen_environment.cpu_view())
        cpu.show_heartbeat = false;
      break;
    case 'c':
      knob_cloudsuite = 1;
      break;
    case 'j':
      knob_json_out = true;
      if (optarg)
        json_file.open(optarg);
    case 0:
      break;
    default:
      abort();
    }
  }

  std::vector<std::string> trace_names{std::next(argv, optind), std::next(argv, argc)};

  std::vector<champsim::tracereader> traces;
  std::transform(std::begin(trace_names), std::end(trace_names), std::back_inserter(traces),
                 [knob_cloudsuite, i = uint8_t(0)](auto name) mutable { return get_tracereader(name, i++, knob_cloudsuite); });

  std::vector<champsim::phase_info> phases{
      {champsim::phase_info{"Warmup", true, warmup_instructions, std::vector<std::size_t>(std::size(trace_names), 0), trace_names},
       champsim::phase_info{"Simulation", false, simulation_instructions, std::vector<std::size_t>(std::size(trace_names), 0), trace_names}}};

  for (auto& p : phases)
    std::iota(std::begin(p.trace_index), std::end(p.trace_index), 0);

  std::cout << std::endl;
  std::cout << "*** ChampSim Multicore Out-of-Order Simulator ***" << std::endl;
  std::cout << std::endl;
  std::cout << "Warmup Instructions: " << phases[0].length << std::endl;
  std::cout << "Simulation Instructions: " << phases[1].length << std::endl;
  std::cout << "Number of CPUs: " << std::size(gen_environment.cpu_view()) << std::endl;
  std::cout << "Page size: " << PAGE_SIZE << std::endl;
  std::cout << std::endl;

  auto phase_stats = champsim::main(gen_environment, phases, traces);

  std::cout << std::endl;
  std::cout << "ChampSim completed all CPUs" << std::endl;
  std::cout << std::endl;

  champsim::plain_printer default_print{std::cout};
  default_print.print(phase_stats);

  for (CACHE& cache : gen_environment.cache_view())
    cache.impl_prefetcher_final_stats();

  for (CACHE& cache : gen_environment.cache_view())
    cache.impl_replacement_final_stats();

  if (knob_json_out) {
    if (json_file.is_open()) {
      champsim::json_printer printer{json_file};
      printer.print(phase_stats);
    } else {
      champsim::json_printer printer{std::cout};
      printer.print(phase_stats);
    }
  }

  return 0;
}
