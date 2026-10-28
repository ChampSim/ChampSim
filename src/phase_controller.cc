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

#include "phase_controller.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <nlohmann/json.hpp>

#include "hooks.h"
#include "module_lifecycle.h"
#include "modules.h"
#include "stat_report.h"

// Defined (in the global namespace) in champsim.cc; the wall-clock stamp appended to completion reports.
std::chrono::seconds elapsed_time();

namespace champsim::modules
{
// --- Interface-provided mechanics, shared by every phase_controller implementation. ---

phase_controller::phase_controller(environment_module* env, std::vector<champsim::module_lifecycle*> governed) : env_(env), governed_(std::move(governed))
{
  // An unspecified governed set means all module_lifecycle modules in the environment.
  if (governed_.empty()) {
    for (auto& mp : env_->typed_view<champsim::module_lifecycle>("module_lifecycle")) {
      governed_.push_back(&mp.get());
    }
  }

  // The governed set is fixed for the run: identify its packet_consumers once, here, so advance()'s
  // per-cycle completion/health/deadlock checks iterate this cached vector instead of dynamic_cast-ing
  // (and heap-allocating a fresh vector) every cycle.
  for (auto* mp : governed_) {
    if (auto* sc = dynamic_cast<packet_consumer*>(mp)) {
      governed_consumers_.emplace_back(*sc);
    }
  }
}

void phase_controller::begin_phase_on_modules(const champsim::phase_info& phase) const
{
  for (auto* mp : governed_) {
    mp->begin_phase(phase.is_warmup);
  }
  champsim::hooks::phase_begin.emit(phase);
}

void phase_controller::end_phase_on_modules(const champsim::phase_info& phase)
{
  // Every governed module ends the phase and reports it in the same call. A module that reports
  // nothing publishes no statistics (a PTW is phase-aware yet has none).
  std::vector<std::pair<champsim::module_lifecycle*, champsim::stat_report>> reports;
  for (auto* mp : governed_) {
    champsim::stat_report report;
    mp->end_phase(report);
    if (!report.empty()) {
      reports.emplace_back(mp, std::move(report));
    }
  }

  if (!phase.roi) {
    return; // unmeasured: the modules still ended the phase, but nothing is collected from it
  }

  champsim::phase_stats stats;
  stats.name = phase.name;

  // Workload identity comes from this controller's own consumers, each naming what feeds it. Scoped
  // like the rest of the collection, so two controllers do not each report every workload.
  for (auto& sc : governed_consumers()) {
    for (auto& desc : sc.get().producer_descriptions()) {
      stats.workloads.emplace_back(sc.get().consumer_id(), std::move(desc));
    }
  }

  // The flat plaintext stat lines are emitted in this order; group by interface (stable within one)
  // so it matches the environment's alphabetical-by-interface view order.
  std::stable_sort(std::begin(reports), std::end(reports),
                   [](const auto& lhs, const auto& rhs) { return lhs.first->stat_interface() < rhs.first->stat_interface(); });

  stats.stats = nlohmann::json::object();
  for (const auto& [mp, report] : reports) {
    stats.lines.insert(stats.lines.end(), report.text().begin(), report.text().end());
    stats.stats[mp->stat_interface()][mp->stat_model()][mp->stat_name()] = report.json_object();
  }

  completed_stats_ = std::move(stats);
}

std::optional<champsim::phase_stats> phase_controller::take_phase_stats()
{
  auto collected = std::move(completed_stats_);
  completed_stats_.reset();
  return collected;
}

} // namespace champsim::modules

namespace
{

// Generic, packet-agnostic phase controller owning completion/deadlock/health
// mechanics; per-consumer policy (e.g. livelock rate) lives in check_health.
// Params: deadlock_cycles, health_period, eof_policy
// (complete_all|complete_consumer), phases[] or warmup_length/simulation_length,
// governs[] (@-refs to the module_lifecycle-havers it governs; default all).
class default_phase_controller : public champsim::modules::phase_controller
{
  using consumer_health = champsim::modules::packet_consumer::consumer_health;

  // Configuration (from builder)
  int deadlock_cycles_ = 500;
  uint64_t health_period_ = 10000000;
  bool complete_all_on_eof_ = true;

  // Owned phase list and a cursor into it; advance() steps the cursor when it begins a phase.
  std::vector<champsim::phase_info> phases_;
  std::vector<champsim::phase_info>::const_iterator current_phase_;
  bool phase_begun_ = false; // false between phases: the next advance() begins one

  int stalled_cycles_ = 0;
  uint64_t health_timer_ = 0;
  bool health_abort_ = false;

  // Consumer tracking: keyed by consumer_id from packet_consumer
  std::map<int, bool> consumer_complete_;
  std::map<int, uint64_t> progress_baseline_;
  std::vector<unsigned> newly_completed_;

  static std::vector<champsim::module_lifecycle*> parse_governed(champsim::modules::ModuleBuilder& builder)
  {
    if (builder.has_parameter("governs")) {
      return builder.get_parameter<std::vector<champsim::module_lifecycle*>>("governs");
    }
    return {}; // empty => the interface governs all module_lifecycle modules
  }

  static std::vector<champsim::phase_info> parse_phases(champsim::modules::ModuleBuilder& builder)
  {
    std::vector<champsim::phase_info> phases;
    if (builder.has_parameter("phases")) {
      for (auto& p : builder.get_parameter<nlohmann::json>("phases")) {
        champsim::phase_info pi;
        pi.name = p.value("name", "Phase");
        pi.is_warmup = p.value("is_warmup", false);
        pi.roi = p.value("roi", !pi.is_warmup);
        pi.length = p.value("length", uint64_t{0});
        phases.push_back(pi);
      }
    } else if (builder.has_parameter("warmup_length") || builder.has_parameter("simulation_length")) {
      uint64_t wlen = builder.get_parameter<uint64_t>("warmup_length", true, 0ULL);
      uint64_t slen = builder.get_parameter<uint64_t>("simulation_length", true, 0ULL);
      phases = {
          champsim::phase_info{"Warmup", true, false, wlen},
          champsim::phase_info{"Simulation", false, true, slen},
      };
    }
    // If neither is set, phases stays empty — the caller owns the phase list.
    return phases;
  }

  // Emit this controller's own completion reports over its governed consumers: producer-finish lines for
  // the consumers that completed this advance(), plus phase-complete lines when the phase just ended.
  // advance() calls this itself, so the orchestrator prints nothing and controllers never cross-report.
  void report_completions(bool phase_ended) const
  {
    for (unsigned id : newly_completed_) {
      for (auto& sc : governed_consumers()) {
        if (sc.get().consumer_id() == static_cast<int>(id)) {
          auto msg = sc.get().producer_finish_message(current_phase_->name);
          if (!msg.empty()) {
            fmt::print("{} (Simulation time: {:%H hr %M min %S sec})\n", msg, ::elapsed_time());
          }
        }
      }
    }
    if (phase_ended) {
      for (auto& sc : governed_consumers()) {
        auto msg = sc.get().phase_complete_message(current_phase_->name);
        if (!msg.empty()) {
          fmt::print("{} (Simulation time: {:%H hr %M min %S sec})\n", msg, ::elapsed_time());
        }
      }
    }
  }

public:
  explicit default_phase_controller(champsim::modules::ModuleBuilder builder)
      : phase_controller(builder.get_parent<champsim::modules::environment_module>(), parse_governed(builder)), phases_(parse_phases(builder)),
        current_phase_(std::cend(phases_)) // sentinel: not yet started
  {
    // Default to the environment's dynamic threshold; a config "deadlock_cycles" overrides.
    deadlock_cycles_ = builder.get_parameter<int>("deadlock_cycles", true, builder.get_parent<champsim::modules::environment_module>()->get_deadlock_cycles());
    health_period_ = builder.get_parameter<uint64_t>("health_period", true, 10000000ULL);
    complete_all_on_eof_ = builder.get_parameter<std::string>("eof_policy", true, std::string{"complete_all"}) != "complete_consumer";

    if (phases_.empty()) {
      fmt::print("ERROR: phase controller declares no phases (set \"phases\", or warmup_length/simulation_length)\n");
      std::exit(-1);
    }
  }

  status advance(long progress) override
  {
    newly_completed_.clear();

    // advance() owns both phase edges. Between phases it steps to the next one and informs the
    // governed modules (their warmup flag must be set before this phase's first operated cycle), then
    // returns without consuming this call's progress. The orchestrator re-calls advance() to start a
    // phase — after it has collected the ended phase's stats — so it never begins/ends phases itself.
    if (!phase_begun_) {
      // Between phases: step to the next one and begin it, or report DONE when none remain.
      auto next_phase = (current_phase_ == std::cend(phases_)) ? std::cbegin(phases_) : std::next(current_phase_);
      if (next_phase == std::cend(phases_)) {
        return status::DONE;
      }
      current_phase_ = next_phase;
      begin_phase_on_modules(*current_phase_);
      stalled_cycles_ = 0;
      health_timer_ = 0;
      health_abort_ = false;
      consumer_complete_.clear();
      for (auto& sc : governed_consumers()) {
        int idx = sc.get().consumer_id();
        if (idx >= 0) {
          consumer_complete_[idx] = false;
          progress_baseline_[idx] = sc.get().sim_progress();
        }
        sc.get().reset_health();
      }
      phase_begun_ = true;
      return status::CONTINUE;
    }

    // Deadlock: consecutive zero-progress cycles are a hang. Work that is scheduled but produces no
    // visible retirement (e.g. an in-flight DRAM refresh) reports itself through progress, so a plain
    // zero-progress count suffices here.
    stalled_cycles_ = (progress == 0) ? stalled_cycles_ + 1 : 0;

    // Health aggregation: each consumer judges itself over the window.
    if (++health_timer_ >= health_period_) {
      for (auto& sc : governed_consumers()) {
        if (sc.get().consumer_id() < 0) {
          continue;
        }
        auto health = sc.get().check_health(health_period_);
        if (health == consumer_health::stalled) {
          fmt::print("{} consumer {} reported stalled\n", current_phase_->name, sc.get().consumer_id());
          health_abort_ = true;
        }
      }
      health_timer_ = 0;
    }

    if (stalled_cycles_ >= deadlock_cycles_ || health_abort_) {
      return status::ABORT;
    }

    // Completion: per-producer EOF (policy-dependent) and progress thresholds.
    for (auto& sc : governed_consumers()) {
      int idx = sc.get().consumer_id();
      if (idx < 0) {
        continue;
      }
      if (consumer_complete_[idx]) {
        continue;
      }

      if (sc.get().producers_eof()) {
        if (complete_all_on_eof_) {
          // Classic behavior: the first exhausted producer ends the phase for everyone.
          for (auto& [other_idx, complete] : consumer_complete_) {
            if (!complete) {
              complete = true;
              newly_completed_.push_back(static_cast<unsigned>(other_idx));
            }
          }
          break;
        }
        consumer_complete_[idx] = true;
        newly_completed_.push_back(static_cast<unsigned>(idx));
        continue;
      }

      if ((sc.get().sim_progress() - progress_baseline_[idx]) >= current_phase_->length) {
        consumer_complete_[idx] = true;
        newly_completed_.push_back(static_cast<unsigned>(idx));
      }
    }

    bool all_complete =
        !consumer_complete_.empty() && std::all_of(consumer_complete_.begin(), consumer_complete_.end(), [](const auto& p) { return p.second; });
    if (all_complete) {
      // Phase complete: end it on the governed modules and return to the "between phases" state. The
      // orchestrator collects this phase's stats, then calls advance() again to begin the next (or to
      // get DONE if this was the last).
      end_phase_on_modules(*current_phase_);
      phase_begun_ = false;
    }

    // The controller reports its own completions (producer-finish, and phase-complete when it just ended).
    report_completions(all_complete);
    return all_complete ? status::COMPLETE : status::CONTINUE;
  }

  const champsim::phase_info& phase() const override { return *current_phase_; }

  std::vector<unsigned> newly_completed_consumers() const override { return newly_completed_; }

  void print_phase_plan() const override
  {
    std::map<std::string, std::vector<int>> ids_by_unit;
    for (auto& sc : governed_consumers())
      ids_by_unit[sc.get().progress_unit()].push_back(sc.get().consumer_id());
    if (ids_by_unit.empty())
      ids_by_unit.emplace("packets", std::vector<int>{});

    const bool disambiguate = ids_by_unit.size() > 1;
    for (const auto& p : phases_)
      for (const auto& [unit, ids] : ids_by_unit)
        if (disambiguate)
          fmt::print("{} {}: {} (consumers {})\n", p.name, unit, p.length, fmt::join(ids, ", "));
        else
          fmt::print("{} {}: {}\n", p.name, unit, p.length);
  }
};

static champsim::modules::phase_controller::register_interface phase_controller_iface_reg("phase_controller", "phase controllers");
static champsim::modules::phase_controller::register_module<default_phase_controller> default_pc_reg("PHASE_CONTROLLER");

} // anonymous namespace
