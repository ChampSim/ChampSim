#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"
#include "champsim_constants.h"

SCENARIO("A cache invalidates an entry in its upper levels when it receives an RFO") {
  GIVEN("A second-level cache with one filled entry") {
    constexpr uint64_t hit_latency = 4;
    constexpr uint64_t miss_latency = 3;
    constexpr uint64_t fill_latency = 2;

    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul_for_seed;
    to_rq_MRP mock_ul_for_test;

    champsim::channel seed_to_ll{};
    champsim::channel test_to_ll{};

    CACHE seed_ul{CACHE::Builder{champsim::defaults::default_l1d}
      .name("407-seed")
      .upper_levels({&mock_ul_for_seed.queues})
      .lower_level(&seed_to_ll)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
    };

    CACHE test_ul{CACHE::Builder{champsim::defaults::default_l1d}
      .name("407-uut")
      .upper_levels({&mock_ul_for_test.queues})
      .lower_level(&test_to_ll)
    };

    CACHE test_ll{CACHE::Builder{champsim::defaults::default_l2c}
      .name("407-ll")
      .upper_levels({&seed_to_ll, &test_to_ll})
      .lower_level(&mock_ll.queues)
      .hit_latency(miss_latency)
    };

    std::array<champsim::operable*, 6> elements{{&seed_ul, &test_ul, &test_ll, &mock_ll, &mock_ul_for_seed, &mock_ul_for_test}};

    // Initialize the prefetching and replacement
    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    // Create a seed packet
    static uint64_t id = 1;
    decltype(mock_ul_for_seed)::request_type seed;
    seed.address = 0xdeadbeef;
    seed.is_translated = true;
    seed.instr_id = id++;
    seed.cpu = 0;
    seed.type = access_type::LOAD;

    // Issue it to the uut
    auto seed_result = mock_ul_for_seed.issue(seed);
    THEN("This issue is received") {
      REQUIRE(seed_result);
    }

    // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
    for (auto i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("An RFO packet is issued") {
      decltype(mock_ul_for_test)::request_type test;
      test.address = seed.address;
      test.is_translated = true;
      test.instr_id = id++;
      test.cpu = 0;
      test.type = access_type::RFO;

      // Issue it to the uut
      auto test_result = mock_ul_for_test.issue(test);
      THEN("This issue is received") {
        REQUIRE(test_result);
      }

      // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      AND_WHEN("The seed packet is re-issued") {
        auto reseed = seed;
        reseed.instr_id = id++;

        auto reseed_result = mock_ul_for_seed.issue(reseed);
        THEN("This issue is received") {
          REQUIRE(reseed_result);
        }

        for (uint64_t i = 0; i < 100; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The re-issue misses the cache") {
          REQUIRE_THAT(mock_ul_for_seed.packets, Catch::Matchers::SizeIs(2));
          REQUIRE(mock_ul_for_seed.packets.back().return_time - mock_ul_for_seed.packets.back().issue_time == (fill_latency + miss_latency + hit_latency + 1)); // +1 due to ordering of elements
        }
      }
    }
  }
}
