#include <catch.hpp>

#include "../../../prefetcher/next_line/next_line.h"
#include "cache.h"
#include "defaults.hpp"
#include "matchers.hpp"
#include "mocks.hpp"

SCENARIO("The next line prefetcher issues prefetches")
{
  GIVEN("An empty cache")
  {
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::modules::ModuleBuilder{"t451_cache", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
      .add_parameter("mshr_size", static_cast<uint32_t>(8))
      .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
      .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
      .add_submodule("prefetcher", champsim::modules::ModuleBuilder{"t451_next_line", "next_line"})
    };

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    WHEN("A packet is issued")
    {
      // Create a test packet
      static uint64_t id = 1;
      decltype(mock_ul)::request_type seed;
      seed.address = champsim::address{0xffff'003f};
      seed.instr_id = id++;
      seed.origin = champsim::origin{0, 0};

      // Issue it to the uut
      auto seed_result = mock_ul.issue(seed);
      THEN("The issue is accepted") { REQUIRE(seed_result); }

      // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("All of the issued requests have the same stride")
      {
        REQUIRE_THAT(mock_ll.addresses, Catch::Matchers::SizeIs(2) && champsim::test::StrideMatcher<champsim::block_number>{1});
      }
    }
  }
}
