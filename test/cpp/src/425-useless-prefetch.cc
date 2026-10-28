#include <catch.hpp>
#include <cstdlib>

#include "cache.h"
#include "defaults.hpp"
#include "mocks.hpp"

SCENARIO("A cache increments the useless prefetch count when it evicts an unhit prefetch")
{
  GIVEN("An empty cache")
  {
    constexpr auto hit_latency = 4;
    constexpr auto miss_latency = 3;
    do_nothing_MRC mock_ll;
    to_wq_MRP mock_ul_seed;
    to_rq_MRP mock_ul_test;
    CACHE uut{champsim::modules::ModuleBuilder{"t425_cache", "DEFAULT_CACHE", champsim::defaults::default_l2c()}
                  .add_parameter("mshr_size", static_cast<uint32_t>(8))
                  .add_parameter("num_sets", static_cast<uint32_t>(1))
                  .add_parameter("num_ways", static_cast<uint32_t>(1))
                  .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul_seed.queues, &mock_ul_test.queues})
                  .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                  .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
                  .add_parameter("fill_latency", static_cast<uint64_t>(miss_latency))};

    std::array<champsim::operable*, 4> elements{{&uut, &mock_ll, &mock_ul_seed, &mock_ul_test}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    // Run the uut for a few cycles
    for (auto i = 0; i < 10; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("A packet is sent")
    {
      const champsim::address seed_addr{0xdeadbeef};
      long old_miss_cycles = uut.sim_stats.total_miss_latency_cycles;
      auto seed_result = uut.prefetch_line(seed_addr, true, 0);

      for (auto i = 0; i < 2 * (miss_latency + hit_latency); ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The issue is received")
      {
        CHECK(seed_result);
        CHECK_THAT(mock_ll.addresses, Catch::Matchers::RangeEquals(std::vector({seed_addr})));
      }

      THEN("The average miss latency does not change") { REQUIRE(uut.sim_stats.total_miss_latency_cycles == old_miss_cycles); }

      AND_WHEN("A packet with a different address is sent")
      {
        decltype(mock_ul_test)::request_type test_b;
        test_b.address = champsim::address{0xcafebabe};
        test_b.origin = champsim::origin{0, 0};
        test_b.type = access_type::LOAD;
        test_b.instr_id = 1;

        auto test_b_result = mock_ul_test.issue(test_b);

        for (auto i = 0; i < hit_latency + 2; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The issue is received")
        {
          CHECK(test_b_result);
          CHECK_THAT(mock_ll.addresses, Catch::Matchers::RangeEquals(std::vector({seed_addr, test_b.address})));
        }

        for (auto i = 0; i < 2 * (miss_latency + hit_latency); ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("It takes exactly the specified cycles to return")
        {
          REQUIRE_THAT(mock_ul_test.packets.front(), champsim::test::ReturnedMatcher(miss_latency + hit_latency + 1, 1));
        }

        THEN("The number of useless prefetches is increased") { REQUIRE(uut.sim_stats.pf_useless == 1); }
      }
    }
  }
}
