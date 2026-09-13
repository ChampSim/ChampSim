#include <catch.hpp>

#include "cache.h"
#include "defaults.hpp"
#include "mocks.hpp"

SCENARIO("A cache returns a hit after the specified latency")
{
  using namespace std::literals;
  auto [type, str] = GENERATE(table<access_type, std::string_view>({std::pair{access_type::LOAD, "load"sv}, std::pair{access_type::RFO, "RFO"sv},
                                                                    std::pair{access_type::PREFETCH, "prefetch"sv}, std::pair{access_type::WRITE, "write"sv},
                                                                    std::pair{access_type::TRANSLATION, "translation"sv}}));

  GIVEN("An empty cache")
  {
    constexpr auto hit_latency = 7;
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::modules::ModuleBuilder{"t401_cache", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
                  .add_parameter("mshr_size", static_cast<uint32_t>(8))
                  .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                  .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                  .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
                  .add_parameter("pref_activate_mask", std::vector<access_type>{access_type::LOAD, access_type::RFO, access_type::PREFETCH, access_type::WRITE, access_type::TRANSLATION})};

    std::array<champsim::operable*, 3> elements{{&uut, &mock_ll, &mock_ul}};

    // Initialize the prefetching and replacement
    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    WHEN("A " + std::string{str} + " packet is issued")
    {
      // Create a test packet
      static uint64_t id = 1;
      decltype(mock_ul)::request_type seed;
      seed.address = champsim::address{0xdeadbeef};
      seed.is_translated = true;
      seed.instr_id = id++;
      seed.origin = champsim::origin{0, 0};
      seed.type = type;

      // Issue it to the uut
      auto seed_result = mock_ul.issue(seed);
      THEN("This issue is received") { REQUIRE(seed_result); }

      // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      AND_WHEN("A packet with the same address is sent")
      {
        auto test = seed;
        test.instr_id = id++;

        const auto initial_hits = uut.sim_stats.hits.value_or(std::pair{test.type, test.origin.cpu()}, 0);

        auto test_result = mock_ul.issue(test);
        THEN("This issue is received") { REQUIRE(test_result); }

        for (uint64_t i = 0; i < 2 * hit_latency; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("It takes exactly the specified cycles to return")
        {
          REQUIRE_THAT(mock_ul.packets, Catch::Matchers::SizeIs(2));
          REQUIRE_THAT(mock_ul.packets.back(), champsim::test::ReturnedMatcher(hit_latency, 1));
        }

        THEN("The number of hits increases") { REQUIRE(uut.sim_stats.hits.value_or(std::pair{test.type, test.origin.cpu()}, 0) == initial_hits + 1); }
      }
    }
  }
}
