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

#ifndef PHASE_CONTROLLER_H
#define PHASE_CONTROLLER_H

#include <functional>
#include <optional>
#include <vector>

#include "module_lifecycle.h"
#include "modules.h"
#include "phase_info.h"

namespace champsim::modules
{
/**
 * Phase controller interface — owns and sequences the run's phases.
 *
 * A controller governs a set of module_lifecycle-havers (the modules whose stats reset on phase edges),
 * given at construction as config @-references (or all of them when unspecified). Distinct
 * controllers must govern disjoint sets so no module hears conflicting phase edges. begin_phase()
 * steps to the next phase and advance() drives the current one; the orchestrator only ticks the
 * operables, collects stats when a phase ends, and ends the run once every controller is DONE.
 *
 * The interface owns the mechanics an implementation shouldn't re-derive: broadcasting a phase edge
 * to exactly the governed modules (begin_phase_on_modules/end_phase_on_modules) and exposing the
 * consumers among them (governed_consumers). An implementation decides only policy: the phase plan
 * and completion.
 */
struct phase_controller : public module_base<phase_controller, environment_module> {
  // advance()'s verdict, and exactly what the orchestrator does with it:
  //   CONTINUE — step the sim (keep ticking)
  //   COMPLETE — a phase ended: collect its stats, then advance() again to begin the next
  //   DONE     — this controller has no phases left: stop it cleanly
  //   ABORT    — deadlock/stall: stop the sim early
  enum class status { CONTINUE, COMPLETE, ABORT, DONE };

  // The module_lifecycle-havers this controller governs; an empty set means all of them in the environment.
  phase_controller(environment_module* env, std::vector<champsim::module_lifecycle*> governed);
  virtual ~phase_controller() = default;

  // The sole driver. advance() owns both phase edges: between phases it begins the next one (informing
  // the governed modules) and returns CONTINUE; while running it consumes the cycle's summed progress
  // and, on completing the phase, ends it on the modules and returns COMPLETE (or DONE after the
  // last phase), ABORT on deadlock/stall, else CONTINUE. The orchestrator only ticks the operables,
  // calls advance(), and collects a phase's stats between its COMPLETE and the next advance().
  virtual status advance(long progress) = 0;

  // The current phase; its name/roi drive the orchestrator's stat collection.
  virtual const champsim::phase_info& phase() const = 0;

  // Ids of consumers that newly completed since the last advance().
  virtual std::vector<unsigned> newly_completed_consumers() const = 0;

  // Print this controller's phase plan at startup (labelled by the unit its consumers report).
  virtual void print_phase_plan() const {}

  // The module_lifecycle-havers this controller governs — for startup validation that governance sets
  // are disjoint and cover every module_lifecycle module.
  const std::vector<champsim::module_lifecycle*>& governed_modules() const { return governed_; }

  // The stats of the phase this controller just completed, or nothing when that phase was
  // unmeasured. Taking them clears them, so each completed phase is collected exactly once.
  std::optional<champsim::phase_stats> take_phase_stats();

protected:
  // Broadcast a phase edge to exactly the governed module_lifecycle modules. Not virtual: implementations
  // call these; which modules to inform is the interface's business, not theirs.
  void begin_phase_on_modules(const champsim::phase_info& phase) const;
  // Ends the phase on every governed module and collects what each one reports. A measured phase's
  // stats are stashed for take_phase_stats(); an unmeasured phase's are discarded, but every module
  // is ended either way.
  void end_phase_on_modules(const champsim::phase_info& phase);

  // The packet_consumers among the governed module_lifecycle-havers. Cast once at construction (the
  // governed set is fixed for the run), not re-derived on every advance().
  const std::vector<std::reference_wrapper<packet_consumer>>& governed_consumers() const { return governed_consumers_; }

private:
  environment_module* env_;
  std::vector<champsim::module_lifecycle*> governed_;
  std::vector<std::reference_wrapper<packet_consumer>> governed_consumers_;
  std::optional<champsim::phase_stats> completed_stats_;
};
} // namespace champsim::modules

#endif
