#include <catch.hpp>

#include "cache.h"
#include "defaults.hpp"
#include "mocks.hpp"

SCENARIO("Duplicate prefetches do not count each other as useful")
{
  GIVEN("An empty cache")
  {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::modules::ModuleBuilder{"t424_cache", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
                  .add_parameter("mshr_size", static_cast<uint32_t>(8))
                  .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                  .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                  .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
                  .add_parameter("fill_latency", static_cast<uint64_t>(fill_latency))};

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    THEN("The number of prefetches is zero")
    {
      REQUIRE(uut.sim_stats.pf_issued == 0);
      REQUIRE(uut.sim_stats.pf_useful == 0);
      REQUIRE(uut.sim_stats.pf_fill == 0);
    }

    WHEN("A prefetch is issued")
    {
      champsim::address seed_addr{0xdeadbeef};
      auto seed_result = uut.prefetch_line(seed_addr, true, 0);

      THEN("The issue is accepted") { REQUIRE(seed_result); }

      // Run the uut for a bunch of cycles to clear it out of the PQ and fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The number of prefetch fills is incremented") { REQUIRE(uut.sim_stats.pf_fill == 1); }

      AND_WHEN("Another prefetch with the same address is sent")
      {
        auto test_result = uut.prefetch_line(seed_addr, true, 0);
        THEN("The issue is accepted") { REQUIRE(test_result); }

        for (uint64_t i = 0; i < 2 * hit_latency; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The number of issued prefetches is incremented") { REQUIRE(uut.sim_stats.pf_issued == 2); }

        THEN("The number of useful prefetches is not incremented") { REQUIRE(uut.sim_stats.pf_useful == 0); }
      }
    }
  }
}
