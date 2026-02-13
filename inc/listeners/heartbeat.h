#ifndef HEARTBEAT_H
#define HEARTBEAT_H

#include <chrono>
#include <deque>
#include <iostream>
#include <vector>
#include <fmt/chrono.h>

#include "events.h"
#include "instruction.h"

class Heartbeat
{
public:
  std::ostream* std_out;

  explicit Heartbeat(std::ostream* so) { std_out = so; }

  static constexpr auto cli_key = "Heartbeat";

  uint64_t cycles_between_printouts = 10000000;
  std::vector<uint64_t> num_retired_last_printout;
  std::vector<uint64_t> cycles_last_printout;
  std::vector<uint64_t> num_retired;

  std::vector<uint64_t> num_retired_start_phase;
  std::vector<uint64_t> cycles_start_phase;
  std::vector<bool> switched_phase; // true if there has been a begin_phase event since the last time RETIRE occurred

  template <Event e, typename... Args>
  void handle_event(Args&&... args);

  void add_cpu(uint32_t cpu)
  {
    while (cpu >= switched_phase.size()) {
      num_retired_last_printout.push_back(0);
      cycles_last_printout.push_back(0);
      num_retired.push_back(0);
      num_retired_start_phase.push_back(0);
      cycles_start_phase.push_back(0);
      switched_phase.push_back(false);
    }
  }
};

std::chrono::seconds elapsed_time();

namespace heartbeat
{

template <Event e, typename... Args>
inline void handle_event(Heartbeat* hb, Args&... args)
{
  // std::cout << "WARNING: generic handle event\n";
}

template <>
inline void handle_event<Event::BEGIN_PHASE>(Heartbeat* hb, [[maybe_unused]] bool& is_warmup)
{
  for (size_t i = 0; i < hb->switched_phase.size(); i++) {
    hb->switched_phase[i] = true;
  }
}

template <>
inline void handle_event<Event::RETIRE>(Heartbeat* hb, uint32_t& cpu, std::deque<ooo_model_instr>::const_iterator& begin,
                                        std::deque<ooo_model_instr>::const_iterator& end, uint64_t& current_cycles)
{
  hb->add_cpu(cpu);
  hb->num_retired[cpu] += std::distance(begin, end);

  if (hb->switched_phase[cpu]) {
    hb->switched_phase[cpu] = false;
    hb->num_retired_start_phase[cpu] = hb->num_retired[cpu];
    hb->cycles_start_phase[cpu] = current_cycles;
  }

  if (hb->num_retired[cpu] >= hb->num_retired_last_printout[cpu] + hb->cycles_between_printouts) {

    auto heartbeat_instr = static_cast<double>(hb->num_retired[cpu] - hb->num_retired_last_printout[cpu]);
    auto heartbeat_cycle = static_cast<double>(current_cycles - hb->cycles_last_printout[cpu]);

    auto phase_instr = static_cast<double>(hb->num_retired[cpu] - hb->num_retired_start_phase[cpu]);
    auto phase_cycle = static_cast<double>(current_cycles - hb->cycles_start_phase[cpu]);

    fmt::print(*(hb->std_out),
               "Heartbeat CPU {} instructions: {} cycles: {} heartbeat IPC: {:.4} cumulative IPC: {:.4} (Simulation time: {:%H hr %M min %S sec})\n", cpu,
               hb->num_retired[cpu], current_cycles, heartbeat_instr / heartbeat_cycle, phase_instr / phase_cycle, elapsed_time());

    hb->num_retired_last_printout[cpu] = hb->num_retired[cpu];
    hb->cycles_last_printout[cpu] = current_cycles;
  }
}

} // namespace heartbeat

template <Event e, typename... Args>
void Heartbeat::handle_event(Args&&... args)
{
  heartbeat::handle_event<e>(this, std::forward<Args>(args)...);
}

#endif