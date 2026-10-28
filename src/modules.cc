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

#include "modules.h"

#include <cstdlib>
#include <map>
#include <fmt/core.h>

#include "cache.h"
#include "instruction_producer.h"
#include "module_lifecycle.h"
#include "phase_controller.h"

// Static member definitions
bool champsim::modules::ModuleBuilder::global_dump_enabled_ = false;
std::string champsim::modules::ModuleBuilder::dump_log_;

// Every module_lifecycle module must be governed by exactly one phase controller (disjoint sets that
// cover all of them), so no module hears a phase edge from a controller in a different phase. An
// environment calls this once its phase controllers are built.
void champsim::modules::environment_module::validate_phase_governance() const
{
  auto controllers = typed_view<phase_controller>("phase_controller");
  if (controllers.empty()) {
    return; // the orchestrator reports the missing-controller error
  }

  std::map<champsim::module_lifecycle*, std::size_t> owning_controller;
  std::size_t controller_idx = 0;
  for (auto& controller : controllers) {
    for (auto* mp : controller.get().governed_modules()) {
      if (!owning_controller.try_emplace(mp, controller_idx).second) {
        fmt::print("ERROR: a module_lifecycle module is governed by more than one phase controller; governance sets must be disjoint\n");
        std::exit(-1);
      }
    }
    ++controller_idx;
  }

  std::size_t orphans = 0;
  for (auto& mp : typed_view<champsim::module_lifecycle>("module_lifecycle")) {
    if (owning_controller.find(&mp.get()) == owning_controller.end()) {
      ++orphans;
    }
  }
  if (orphans > 0) {
    fmt::print("ERROR: {} module_lifecycle module(s) are governed by no phase controller; every phase module must be governed\n", orphans);
    std::exit(-1);
  }
}

// Initialize the process-wide globals builder with the system-wide defaults so
// modules constructed without an environment (e.g. unit tests) still get sane
// fall-through values for block_size / page_size / etc.
namespace
{
struct globals_default_initializer {
  globals_default_initializer()
  {
    auto& g = champsim::modules::ModuleBuilder::globals();
    g.add_parameter("block_size", 64u);
    g.add_parameter("page_size", 4096u);
    g.add_parameter("log2_block_size", 6u);
    g.add_parameter("log2_page_size", 12u);
  }
};
static globals_default_initializer _globals_default_init;
} // anonymous namespace

namespace champsim::modules
{

bool prefetcher::prefetch_line(champsim::address pf_addr, bool fill_this_level, uint32_t prefetch_metadata) const
{
  return intern_->prefetch_line(pf_addr, fill_this_level, prefetch_metadata);
}
// LCOV_EXCL_START Exclude deprecated function
bool champsim::modules::prefetcher::prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata) const
{
  return prefetch_line(champsim::address{pf_addr}, fill_this_level, prefetch_metadata);
}
// LCOV_EXCL_STOP

} // namespace champsim::modules

// Interface registrations: map interface name strings to module_base specializations.
// The optional second argument is the interface's human-readable display name -- a general
// label for logging, summaries, and diagnostics; when omitted, callers fall back to the
// interface name.
static champsim::modules::channel_module::register_interface channel_iface_reg("channel", "channels");
static champsim::modules::cache_module::register_interface cache_iface_reg("cache", "caches");
static champsim::modules::memory_controller_module::register_interface memory_controller_iface_reg("memory_controller", "memory controllers");
static champsim::modules::vmem_module::register_interface vmem_iface_reg("vmem", "virtual memories");
static champsim::modules::page_table_walker_module::register_interface ptw_iface_reg("page_table_walker", "page table walkers");
static champsim::modules::core_module::register_interface core_iface_reg("core", "cores");
static champsim::modules::environment_module::register_interface env_iface_reg("environment", "environments");

// Submodule interfaces. These are created by their parent modules (a core
// builds its instruction producers, a cache its prefetchers), not by the
// environment — registering them names the interface for nested-instance
// enrollment and lets environment views cover them.
static champsim::modules::instruction_producer::register_interface instruction_producer_iface_reg("instruction_producer", "instruction producers");
static champsim::modules::prefetcher::register_interface prefetcher_iface_reg("prefetcher", "prefetchers");
static champsim::modules::replacement::register_interface replacement_iface_reg("replacement", "replacement policies");
static champsim::modules::branch_predictor::register_interface branch_predictor_iface_reg("branch_predictor", "branch predictors");
static champsim::modules::btb::register_interface btb_iface_reg("btb", "BTBs");
