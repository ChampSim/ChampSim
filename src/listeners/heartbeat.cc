/*
 *    Copyright 2026 The ChampSim Contributors
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

#include "listeners/heartbeat.h"

#include <chrono>
#include <string>
#include <utility>
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/ostream.h>

#include "hooks.h"
#include "phase_info.h"

// Defined (in the global namespace) in champsim.cc; the wall-clock stamp appended to each line.
std::chrono::seconds elapsed_time();

namespace champsim::listeners
{

heartbeat::heartbeat(champsim::modules::ModuleBuilder builder)
{
  // "frequency" is this listener's own knob; the root "heartbeat_frequency" is the system-wide
  // default it falls through to.
  period_ = builder.get_parameter<uint64_t>("frequency", true, builder.get_parameter<uint64_t>("heartbeat_frequency", true, uint64_t{10000000}));

  progress_sub_ = champsim::hooks::progress.subscribe([this](const champsim::modules::packet_consumer& consumer, const uint64_t& total_progress,
                                                             const uint64_t& total_cycles) { on_progress(consumer, total_progress, total_cycles); });

  phase_sub_ = champsim::hooks::phase_begin.subscribe([this](const champsim::phase_info& /*phase*/) {
    // Rates are reported per phase, so every consumer re-baselines on its next report.
    for (std::size_t i = 0; i < switched_phase_.size(); i++) {
      switched_phase_[i] = true;
    }
  });
}

void heartbeat::track(std::size_t idx)
{
  while (idx >= switched_phase_.size()) {
    last_printout_progress_.push_back(0);
    last_printout_cycles_.push_back(0);
    phase_start_progress_.push_back(0);
    phase_start_cycles_.push_back(0);
    switched_phase_.push_back(false);
  }
}

void heartbeat::on_progress(const champsim::modules::packet_consumer& consumer, uint64_t total_progress, uint64_t total_cycles)
{
  const int id = consumer.consumer_id();
  if (id < 0) {
    return;
  }
  const auto idx = static_cast<std::size_t>(id);
  track(idx);

  if (switched_phase_[idx]) {
    switched_phase_[idx] = false;
    phase_start_progress_[idx] = total_progress;
    phase_start_cycles_[idx] = total_cycles;
  }

  if (total_progress >= last_printout_progress_[idx] + period_) {
    const auto interval_progress = static_cast<double>(total_progress - last_printout_progress_[idx]);
    const auto interval_cycles = static_cast<double>(total_cycles - last_printout_cycles_[idx]);
    const auto phase_progress = static_cast<double>(total_progress - phase_start_progress_[idx]);
    const auto phase_cycles = static_cast<double>(total_cycles - phase_start_cycles_[idx]);

    const auto msg = consumer.progress_message(total_progress, total_cycles, interval_progress / interval_cycles, phase_progress / phase_cycles);
    if (!msg.empty()) {
      fmt::print(*out_, "{} (Simulation time: {:%H hr %M min %S sec})\n", msg, elapsed_time());
    }

    // Advance the baseline by whole interval multiples rather than snapping to the current count:
    // snapping would accumulate per-heartbeat overshoot and drift the schedule.
    const auto overshoot = total_progress - last_printout_progress_[idx];
    last_printout_progress_[idx] += (overshoot / period_) * period_;
    last_printout_cycles_[idx] = total_cycles;
  }
}

static champsim::modules::listener::register_interface listener_iface_reg("listener", "listeners");
static champsim::modules::listener::register_module<heartbeat> heartbeat_reg("HEARTBEAT");

} // namespace champsim::listeners
