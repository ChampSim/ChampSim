#include <catch.hpp>

#include "cache.h"
#include "defaults.hpp"
#include "mocks.hpp"
#include "operable.h"

namespace
{

// A prefetcher that counts its per-cycle hook invocations: the contract is
// once per cache cycle, whether or not the cache skipped its simulation.
struct counting_pref_424 : champsim::modules::prefetcher {
  inline static counting_pref_424* last_instance = nullptr;
  long cycle_calls = 0;

  explicit counting_pref_424(champsim::modules::ModuleBuilder) { last_instance = this; }

  void prefetcher_initialize() override {}
  uint32_t prefetcher_cache_operate(champsim::address, champsim::address, bool, bool, access_type, uint32_t metadata_in) override { return metadata_in; }
  uint32_t prefetcher_cache_fill(champsim::address, long, long, bool, champsim::address, uint32_t metadata_in) override { return metadata_in; }
  void prefetcher_cycle_operate() override { ++cycle_calls; }
  void prefetcher_final_stats() override {}
  void prefetcher_branch_operate(champsim::address, uint8_t, champsim::address) override {}
};

static champsim::modules::prefetcher::register_module<counting_pref_424> pref_reg_424("T424_COUNTING_PREF");

struct skip_guard {
  bool saved = champsim::operable::skip_enabled();
  ~skip_guard() { champsim::operable::set_skip_enabled(saved); }
};

} // namespace

SCENARIO("An idle cache skips its simulation but still ticks its prefetchers every cycle")
{
  auto with_skip = GENERATE(true, false);

  GIVEN("An idle cache with a cycle-counting prefetcher, cycle skip " + std::string{with_skip ? "enabled" : "disabled"})
  {
    skip_guard guard;
    champsim::operable::set_skip_enabled(with_skip);

    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    auto builder = champsim::modules::ModuleBuilder{"t424_cache", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
                       .add_parameter("mshr_size", static_cast<uint32_t>(8))
                       .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                       .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues));
    builder.clear_submodules("prefetcher");
    builder.add_submodule("prefetcher", champsim::modules::ModuleBuilder{"t424_pref", "T424_COUNTING_PREF"});
    CACHE uut{builder};
    auto* pref = counting_pref_424::last_instance;
    REQUIRE(pref != nullptr);

    uut.initialize();
    uut.begin_phase(false);

    WHEN("The cache runs 100 cycles through the orchestrator path with empty queues")
    {
      constexpr int num_cycles = 100;
      champsim::chrono::clock clock;
      for (int i = 0; i < num_cycles; ++i) {
        clock.tick(uut.clock_period);
        uut.operate_on(clock);
      }

      THEN("Time advances one period per cycle regardless of skipping")
      {
        REQUIRE(uut.current_time.time_since_epoch() == num_cycles * uut.clock_period);
      }
      THEN("The prefetcher observes an unbroken per-cycle stream")
      {
        REQUIRE(pref->cycle_calls == num_cycles);
      }
    }
  }
}

SCENARIO("A cache serves a request with identical timing whether or not idle skip is enabled")
{
  auto with_skip = GENERATE(true, false);

  GIVEN("An empty cache with hit latency 7, cycle skip " + std::string{with_skip ? "enabled" : "disabled"})
  {
    skip_guard guard;
    champsim::operable::set_skip_enabled(with_skip);

    constexpr auto hit_latency = 7;
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::modules::ModuleBuilder{"t424b_cache", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
                  .add_parameter("mshr_size", static_cast<uint32_t>(8))
                  .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                  .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                  .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))};

    std::array<champsim::operable*, 3> elements{{&uut, &mock_ll, &mock_ul}};
    for (auto* elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) {
        mp->begin_phase(false);
      }
    }

    // Let the cache idle (and skip, when enabled) before the request arrives.
    // The cache runs through the orchestrator path (operate_on, where the
    // skip hook lives); the mocks are stepped one cycle per cache cycle.
    champsim::chrono::clock clock;
    auto run_cycle = [&]() {
      clock.tick(uut.clock_period);
      uut.operate_on(clock);
      mock_ll._operate();
      mock_ul._operate();
    };
    for (int i = 0; i < 20; ++i) {
      run_cycle();
    }

    WHEN("A load is issued after the idle period")
    {
      decltype(mock_ul)::request_type seed;
      seed.address = champsim::address{0xdeadbeef};
      seed.is_translated = true;
      seed.instr_id = 1;
      seed.origin = champsim::origin{0, 0};
      seed.type = access_type::LOAD;

      auto seed_result = mock_ul.issue(seed);
      REQUIRE(seed_result);

      int cycles_to_return = 0;
      while (mock_ul.packets.back().return_time == 0 && cycles_to_return < 1000) {
        run_cycle();
        ++cycles_to_return;
      }
      REQUIRE(cycles_to_return < 1000);

      THEN("The request is served in the same number of cycles either way")
      {
        // Miss + fill through the zero-latency lower level: the exact count
        // matters less than its independence from the skip setting; pin it.
        static std::map<bool, int> observed;
        observed[with_skip] = cycles_to_return;
        // Assert on every generated pass so this section is never assertion-free
        // (--warn NoAssertions fails an assertion-free section); the cross-skip
        // equality is checked once both passes have recorded their timing.
        REQUIRE(cycles_to_return > 0);
        if (observed.size() == 2) {
          REQUIRE(observed.at(true) == observed.at(false));
        }
      }
    }
  }
}
