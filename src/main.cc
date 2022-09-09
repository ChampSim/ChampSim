#include <getopt.h>
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
#include "ptw.h"
#include "vmem.h"
#include "tracereader.h"

extern MEMORY_CONTROLLER DRAM;
extern std::vector<std::reference_wrapper<O3_CPU>> ooo_cpu;
extern std::vector<std::reference_wrapper<CACHE>> caches;
extern std::vector<std::reference_wrapper<PageTableWalker>> ptws;
extern std::vector<std::reference_wrapper<champsim::operable>> operables;

void init_structures();

struct phase_info {
  std::string name;
  bool is_warmup;
  uint64_t length;
};

int champsim_main(std::vector<std::reference_wrapper<O3_CPU>>& cpus, std::vector<std::reference_wrapper<champsim::operable>>& operables,
                  std::vector<phase_info>& phases, bool knob_cloudsuite, std::vector<std::string> trace_names);

void signal_handler(int signal)
{
  std::cout << "Caught signal: " << signal << std::endl;
  abort();
}

int main(int argc, char** argv)
{
  // interrupt signal hanlder
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = signal_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  // initialize knobs
  uint8_t knob_cloudsuite = 0;
  uint64_t warmup_instructions = 1000000, simulation_instructions = 10000000;

  // check to see if knobs changed using getopt_long()
  int traces_encountered = 0;
  static struct option long_options[] = {{"warmup_instructions", required_argument, 0, 'w'},
                                         {"simulation_instructions", required_argument, 0, 'i'},
                                         {"hide_heartbeat", no_argument, 0, 'h'},
                                         {"cloudsuite", no_argument, 0, 'c'},
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
      for (O3_CPU& cpu : ooo_cpu)
        cpu.show_heartbeat = false;
      break;
    case 'c':
      knob_cloudsuite = 1;
      break;
    case 0:
      break;
    default:
      abort();
    }
  }

  std::vector<std::string> trace_names{std::next(argv, optind), std::next(argv, argc)};

  std::vector<phase_info> phases{{phase_info{"Warmup", true, warmup_instructions}, phase_info{"Simulation", false, simulation_instructions}}};

  std::cout << std::endl;
  std::cout << "*** ChampSim Multicore Out-of-Order Simulator ***" << std::endl;
  std::cout << std::endl;
  std::cout << "Warmup Instructions: " << phases[0].length << std::endl;
  std::cout << "Simulation Instructions: " << phases[1].length << std::endl;
  std::cout << "Number of CPUs: " << std::size(ooo_cpu) << std::endl;
  std::cout << "Page size: " << PAGE_SIZE << std::endl;

  std::cout << std::endl;
  int i = 0;
  for (auto name : trace_names)
    std::cout << "CPU " << i++ << " runs " << name << std::endl;

  if (std::size(trace_names) != std::size(ooo_cpu)) {
    std::cerr << std::endl;
    std::cerr << "*** Number of traces does not match the number of cores ***";
    std::cerr << std::endl;
    std::cerr << std::endl;
    return 1;
  }

  init_structures();

  champsim_main(ooo_cpu, operables, phases, knob_cloudsuite, trace_names);

  std::cout << "ChampSim completed all CPUs" << std::endl;

  if (std::size(ooo_cpu) > 1) {
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
}
