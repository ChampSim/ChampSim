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

#ifndef PACKET_CONSUMER_H
#define PACKET_CONSUMER_H

#include <cstdint>
#include <string>
#include <vector>
#include <fmt/core.h>

namespace champsim::modules
{
// Mixin for any module that consumes packet producers.
// Inherit from this to attach packet_producer submodules.
struct packet_consumer {
  virtual ~packet_consumer() = default;

  // Health as judged by the consumer itself. The consumer knows its own
  // expected progress rate (instructions retired for a core, packets
  // delivered for a network consumer, ...), so livelock policy lives here —
  // not in the phase controller, which only aggregates.
  enum class consumer_health { healthy, warning, critical, stalled };

  // True when all attached packet producers are exhausted.
  virtual bool producers_eof() const { return true; }

  // Consumer id: this consumer's hardware-context identity. Assigned by
  // the framework at startup, by enumeration in configuration order —
  // never written in a configuration. Ids are unique and dense in
  // [0, num_consumers); they key per-consumer resources (replacement
  // tables, stats, phase tracking) and default the producer id of attached
  // producers. See origin.h. Standalone instances (unit tests) keep the
  // default of 0, matching the historical single-core default.
  int consumer_id() const { return consumer_id_; }
  // The instance name this consumer was configured under (set by the
  // module factory). Use champsim::identities() for id <-> name lookups.
  const std::string& consumer_name() const { return identity_name_; }
  void set_identity_name(std::string name) { identity_name_ = std::move(name); }
  void set_consumer_id(int id)
  {
    if (!consumer_id_pinned_) {
      consumer_id_ = id;
    }
  }
  // Consumers that mirror another consumer's identity (rather than being
  // hardware contexts of their own — e.g. a shadow/replay channel) may pin
  // their id; pinned consumers are skipped by the startup enumeration and
  // do not occupy an id slot.
  void pin_consumer_id(int id)
  {
    consumer_id_ = id;
    consumer_id_pinned_ = true;
  }
  bool consumer_id_pinned() const { return consumer_id_pinned_; }

private:
  int consumer_id_ = 0;
  bool consumer_id_pinned_ = false;
  std::string identity_name_{};

public:
  // The workloads feeding this consumer, as its producers describe themselves (e.g. trace paths).
  // Reported against this consumer's id, so a consumer fed by two producers names both against
  // itself rather than being counted twice.
  virtual std::vector<std::string> producer_descriptions() const { return {}; }

  // Progress metric for phase completion, in packets (e.g. instructions
  // retired). Return 0 to indicate no progress tracking (complete only on EOF).
  virtual uint64_t sim_progress() const { return 0; }

  // Periodic self-check, driven by the phase controller every health
  // period. elapsed is the number of controller cycles since the last
  // check (or reset_health). Return stalled to abort the simulation.
  virtual consumer_health check_health(uint64_t /*elapsed*/) { return consumer_health::healthy; }

  // Re-baseline health tracking; called by the controller at phase start.
  virtual void reset_health() {}

  // Called when this consumer's producer finishes a phase. Return empty to suppress.
  virtual std::string producer_finish_message(const std::string& /*phase_name*/) const { return {}; }

  // Called at the end of a phase for summary output. Return empty to suppress.
  virtual std::string phase_complete_message(const std::string& /*phase_name*/) const { return {}; }

  // Format one periodic heartbeat line. The heartbeat listener owns the interval
  // bookkeeping and supplies the numbers; the consumer owns the wording because only it
  // knows its own progress unit (a core reports instructions; other consumers their own).
  // Return empty to suppress.
  virtual std::string progress_message(uint64_t total_progress, uint64_t total_cycles, double interval_rate, double cumulative_rate) const
  {
    return fmt::format("Heartbeat consumer {} progress: {} cycles: {} rate: {:.4} cumulative rate: {:.4}", consumer_id(), total_progress, total_cycles,
                       interval_rate, cumulative_rate);
  }

  // The unit this consumer measures phase progress in (a core reports "instructions").
  virtual std::string progress_unit() const { return "packets"; }
};
} // namespace champsim::modules

#endif
