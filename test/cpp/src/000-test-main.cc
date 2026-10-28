#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include "extent.h"
#include "hook.h"
#include "modules.h"

namespace {
// Reset the process-wide globals builder back to its system defaults so each
// test sees a clean baseline. Env-constructing tests (e.g. 502 with a custom
// block_size / page_size config) write the env's values into the globals as
// part of construction, and would otherwise leak those values into later
// tests that build modules directly without an env (e.g. the 800-series
// vmem tests).
void reset_globals_to_defaults()
{
  auto& g = champsim::modules::ModuleBuilder::globals();
  g.add_parameter("block_size",      64u);
  g.add_parameter("page_size",       4096u);
  g.add_parameter("log2_block_size", 6u);
  g.add_parameter("log2_page_size",  12u);
  // num_consumers is stamped into the globals during env construction and sizes per-consumer
  // replacement state (ship/drrip). Reset it to the single-consumer default so a multi-core config's
  // count cannot leak into a later test that builds modules without that config.
  g.add_parameter("num_consumers", std::size_t{1});
  // Re-sync the cached extents used by page_number / block_offset / etc.
  champsim::refresh_address_extents();
  // Module instances live in a static registry for the life of the binary, so any hook a module
  // subscribed to in one test case would still be calling it in the next. Drop every subscription.
  champsim::hook_registry::clear_all_subscribers();
}

struct globals_reset_listener : Catch::EventListenerBase {
  using EventListenerBase::EventListenerBase;
  void testCaseStarting(const Catch::TestCaseInfo& /*info*/) override { reset_globals_to_defaults(); }
};
} // namespace

CATCH_REGISTER_LISTENER(globals_reset_listener)
