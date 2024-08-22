#include <catch.hpp>
#include "mocks.hpp"
#include "matchers.hpp"
#include "defaults.hpp"
#include "cache.h"

#include "../../../prefetcher/next_line/next_line.h"

SCENARIO("The next line prefetcher issues prefetches") {
  GIVEN("An empty cache") {
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("451-uut")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .prefetcher<next_line>()
    };

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A packet is issued") {
      // Create a test packet
      static uint64_t id = 1;
      decltype(mock_ul)::request_type seed;
      seed.address = champsim::address{0xffff'003f};
      seed.instr_id = id++;
      seed.cpu = 0;

      // Issue it to the uut
      auto seed_result = mock_ul.issue(seed);
      THEN("The issue is accepted") {
        REQUIRE(seed_result);
      }

      // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("All of the issued requests have the same stride") {
        REQUIRE_THAT(mock_ll.addresses, Catch::Matchers::SizeIs(2) && champsim::test::StrideMatcher<champsim::block_number>{1});
      }
    }
  }
}

SCENARIO("The next line prefetcher issues prefetches in a moved-constructed cache") {
  GIVEN("An empty cache") {
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE move_source{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("451-uut")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .prefetcher<next_line>()
    };
    CACHE uut{std::move(move_source)};

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A packet is issued") {
      // Create a test packet
      static uint64_t id = 1;
      decltype(mock_ul)::request_type seed;
      seed.address = champsim::address{0xffff'003f};
      seed.instr_id = id++;
      seed.cpu = 0;

      // Issue it to the uut
      auto seed_result = mock_ul.issue(seed);
      THEN("The issue is accepted") {
        REQUIRE(seed_result);
      }

      // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("All of the issued requests have the same stride") {
        REQUIRE_THAT(mock_ll.addresses, Catch::Matchers::SizeIs(2) && champsim::test::StrideMatcher<champsim::block_number>{1});
      }
    }
  }
}
