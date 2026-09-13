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

#include "champsim.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <numeric>
#include <utility>
#include <vector>
#include <fmt/core.h>

#include "identity_registry.h"
#include "modules.h"
#include "operable.h"
#include "phase_controller.h"
#include "phase_info.h"

const auto start_time = std::chrono::steady_clock::now();

std::chrono::seconds elapsed_time() { return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time); }

namespace champsim
{

// Sort a per-cycle copy (not in place) and operate all operables, so tie-breaks match develop's fresh-copy sort.
long do_cycle(std::vector<std::reference_wrapper<champsim::operable>>& operables, champsim::chrono::clock& global_clock)
{
  auto sorted = operables;
  std::sort(std::begin(sorted), std::end(sorted),
            [](const champsim::operable& lhs, const champsim::operable& rhs) { return lhs.current_time < rhs.current_time; });

  long progress{0};
  for (champsim::operable& op : sorted) {
    progress += op.operate_on(global_clock);
  }

  return progress;
}

// Assign framework-internal identities: consumers enumerate densely in config order; each producer gets its own id unless producers share a
// "producer_group" label. Configs never contain the numbers; only origins carry them.
void assign_identities(modules::environment_module& env)
{
  identities().clear();

  int next_consumer = 0;
  for (auto& sc : env.typed_view<modules::packet_consumer>("packet_consumer")) {
    if (sc.get().consumer_id_pinned()) {
      continue; // mirrors another consumer's identity; owns no slot
    }
    sc.get().set_consumer_id(next_consumer);
    identities().register_consumer(next_consumer, sc.get().consumer_name());
    ++next_consumer;
  }

  auto num_consumers = modules::ModuleBuilder::globals().get_parameter<std::size_t>("num_consumers", true, std::size_t{0});
  if (num_consumers > 0 && static_cast<std::size_t>(next_consumer) > num_consumers) {
    fmt::print("ERROR: {} consumers found but num_consumers is {} — per-consumer tables would index out of bounds. "
               "Remove or raise the root config key \"num_consumers\".\n",
               next_consumer, num_consumers);
    std::exit(-1);
  }

  uint32_t next_producer_group = 0;
  std::map<std::string, uint32_t> producer_group_labels;
  for (auto& src : env.typed_view<modules::packet_producer>("packet_producer")) {
    if (src.get().producer_id_pinned()) {
      continue; // mirrors another producer's id; owns no slot
    }
    const auto& label = src.get().producer_group();
    if (label.empty()) {
      src.get().set_producer_id(next_producer_group);
      identities().register_producer(next_producer_group, src.get().producer_name());
      ++next_producer_group;
    } else {
      auto [it, fresh] = producer_group_labels.try_emplace(label, next_producer_group);
      if (fresh) {
        ++next_producer_group;
      }
      src.get().set_producer_id(it->second);
      identities().register_producer(it->second, src.get().producer_name());
    }
  }

  auto num_producer_groups = modules::ModuleBuilder::globals().get_parameter<std::size_t>("num_producer_groups", true, std::size_t{0});
  if (num_producer_groups > 0 && static_cast<std::size_t>(next_producer_group) > num_producer_groups) {
    fmt::print("ERROR: {} producer groups found but num_producer_groups is {} — per-producer tables would index out of bounds. "
               "Remove or raise the root config key \"num_producer_groups\".\n",
               next_producer_group, num_producer_groups);
    std::exit(-1);
  }

  // Warm each producer's page-table root in producer-id order, so physical page assignment is a pure function of config rather than runtime walk timing
  // (matches historical construction-time order).
  for (auto& vm : env.typed_view<modules::vmem_module>("vmem")) {
    for (uint32_t producer = 0; producer < next_producer_group; ++producer) {
      (void)vm.get().get_pte_pa(champsim::origin{champsim::origin::invalid_id, producer}, champsim::page_number{}, vm.get().get_pt_levels());
    }
  }
}

identity_registry& identities()
{
  static identity_registry registry;
  return registry;
}

// simulation entry point
std::vector<phase_stats> main(modules::environment_module& env)
{
  assign_identities(env);

  for (champsim::operable& op : env.typed_view<champsim::operable>("operable")) {
    op.initialize();
  }

  auto controllers = env.typed_view<modules::phase_controller>("phase_controller");
  if (controllers.empty()) {
    fmt::print("ERROR: no phase controller declared\n");
    return {};
  }
  auto operables = env.typed_view<champsim::operable>("operable");
  const auto time_quantum = std::accumulate(std::cbegin(operables), std::cend(operables), champsim::chrono::clock::duration::max(),
                                            [](const auto acc, const operable& y) { return std::min(acc, y.clock_period); });

  champsim::chrono::clock global_clock;
  std::vector<phase_stats> results;

  // Each controller owns and drives its own phases through advance(): between phases it begins the next
  // one (setting the modules' warmup flag before that phase's first tick); on completion it ends the
  // phase, collects what its governed modules reported, and returns COMPLETE/DONE. main only ticks the
  // operables, calls advance(), takes a completed phase's stats (before re-calling advance() to begin
  // the next), ends the run once every controller is DONE, and aborts if any controller aborts.
  std::vector<bool> finished(controllers.size(), false);
  std::size_t finished_count = 0;

  // Begin each controller's first phase before the first tick.
  for (modules::phase_controller& controller : controllers) {
    controller.advance(0);
  }

  while (finished_count < controllers.size()) {
    global_clock.tick(time_quantum);
    auto progress = do_cycle(operables, global_clock);

    bool any_abort = false;
    std::size_t idx = 0;
    for (modules::phase_controller& controller : controllers) {
      if (finished[idx]) {
        ++idx;
        continue;
      }
      auto phase_status = controller.advance(progress);

      // COMPLETE: a phase ended — take the stats it collected (before the controller begins the next
      // phase and the modules reset), then advance() again to begin the next phase before its first tick.
      if (phase_status == modules::phase_controller::status::COMPLETE) {
        if (auto collected = controller.take_phase_stats(); collected.has_value()) {
          results.push_back(std::move(*collected));
        }
        phase_status = controller.advance(0);
      }

      if (phase_status == modules::phase_controller::status::ABORT) {
        any_abort = true;
      } else if (phase_status == modules::phase_controller::status::DONE) {
        finished[idx] = true;
        ++finished_count;
      }
      // CONTINUE: step the sim.
      ++idx;
    }

    if (any_abort) {
      std::for_each(std::begin(operables), std::end(operables), [](champsim::operable& c) { c.print_deadlock(); });
      abort();
    }
  }

  return results;
}
} // namespace champsim
