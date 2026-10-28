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

#ifndef LISTENERS_HEARTBEAT_H
#define LISTENERS_HEARTBEAT_H

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

#include "hook.h"
#include "modules.h"

namespace champsim::listeners
{

/**
 * Periodic progress output: the "Heartbeat CPU N instructions: ..." line.
 *
 * Packet-agnostic -- it reports whatever a consumer counts, worded by that consumer through
 * packet_consumer::progress_message, so a core reports instructions and another consumer reports
 * its own unit.
 *
 * Parameter: frequency, the progress units between lines. Falls through to the root
 * heartbeat_frequency, and defaults to 10,000,000.
 */
class heartbeat : public champsim::modules::listener
{
public:
  explicit heartbeat(champsim::modules::ModuleBuilder builder);

  // Redirect the output. Production writes to stdout; tests capture it.
  void set_output(std::ostream& stream) { out_ = &stream; }

private:
  void on_progress(const champsim::modules::packet_consumer& consumer, uint64_t total_progress, uint64_t total_cycles);
  void track(std::size_t idx);

  std::ostream* out_ = &std::cout;
  uint64_t period_ = 10000000;

  // Per-consumer bookkeeping, indexed by consumer_id and grown on demand.
  std::vector<uint64_t> last_printout_progress_;
  std::vector<uint64_t> last_printout_cycles_;
  std::vector<uint64_t> phase_start_progress_;
  std::vector<uint64_t> phase_start_cycles_;
  std::vector<bool> switched_phase_; // a phase began since this consumer's last progress report

  champsim::subscription progress_sub_;
  champsim::subscription phase_sub_;
};

} // namespace champsim::listeners

#endif // LISTENERS_HEARTBEAT_H
