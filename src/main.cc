#include <getopt.h>
#include <iostream>
#include <signal.h>
#include <string>
#include <vector>

int champsim_main(uint64_t warmup_instructions, uint64_t simulation_instructions, bool show_heartbeat, bool knob_cloudsuite,
                  std::vector<std::string> trace_names);

void signal_handler(int signal)
{
  std::cout << "Caught signal: " << signal << std::endl;
  abort();
}

struct phase_info {
  std::string name;
  bool is_warmup;
  uint64_t length;
};

int main(int argc, char** argv)
{
  // interrupt signal hanlder
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = signal_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  // initialize knobs
  uint8_t show_heartbeat = 1;
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
      show_heartbeat = 0;
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

  return champsim_main(warmup_instructions, simulation_instructions, show_heartbeat, knob_cloudsuite, trace_names);
}
