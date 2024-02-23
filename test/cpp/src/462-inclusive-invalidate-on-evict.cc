#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"

SCENARIO("An inclusive block invalidates the upper levels on eviction") {
  GIVEN("Two cache levels with a single inclusive filled block") {
    constexpr uint64_t hit_latency = 4;
    constexpr uint64_t miss_latency = 3;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll{miss_latency};
    to_rq_MRP mock_ul_seed;
    to_rq_MRP mock_ul_test;

    champsim::channel seed_to_lower{};
    champsim::channel test_to_lower{};

    CACHE seed_upper{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("462-seed-upper")
      .upper_levels({&mock_ul_seed.queues})
      .lower_level(&seed_to_lower)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
    };

    CACHE test_upper{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("462-test-upper")
      .upper_levels({&mock_ul_test.queues})
      .lower_level(&test_to_lower)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
    };

    CACHE lower{champsim::cache_builder{champsim::defaults::default_l2c}
      .name("462-lower")
      .upper_levels({&seed_to_lower, &test_to_lower})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .sets(1)
      .ways(1)
    };

    std::array<champsim::operable*, 6> elements{{&seed_upper, &test_upper, &lower, &mock_ll, &mock_ul_seed, &mock_ul_test}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    // Create a test packet
    static uint64_t id = 1;
    decltype(mock_ul_seed)::request_type seed;
    seed.address = champsim::address{0xdeadbeef};
    seed.cpu = 0;
    seed.instr_id = id++;
    seed.clusivity = champsim::inclusivity::inclusive;

    // Issue it to the uut
    auto seed_result = mock_ul_seed.issue(seed);
    THEN("This issue is received") {
      REQUIRE(seed_result);
    }

    // Run the uut for long enough to fulfill the request
    for (uint64_t i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("The inclusive block is replaced") {
      decltype(mock_ul_test)::request_type evictor;
      evictor.address = champsim::address{0xcafebabe};
      evictor.cpu = 0;
      evictor.instr_id = id++;

      // Issue it to the uut
      auto evictor_result = mock_ul_test.issue(evictor);
      THEN("This issue is received") {
        REQUIRE(evictor_result);
      }

      // Run the uut for long enough to fulfill the request
      for (uint64_t i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      AND_WHEN("The first request is re-issued") {
        decltype(mock_ul_seed)::request_type test = seed;
        test.instr_id = id++;

        // Issue it to the uut
        auto test_result = mock_ul_test.issue(test);
        THEN("This issue is received") {
          REQUIRE(test_result);
        }

        // Run the uut for long enough to fulfill the request
        for (uint64_t i = 0; i < 100; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The re-issue misses") {
          REQUIRE(mock_ul_seed.packets.back().return_time == mock_ul_seed.packets.back().issue_time + (2*fill_latency + miss_latency + 2*hit_latency + 2)); // +1 due to ordering of elements
        }
      }
    }
  }
}

