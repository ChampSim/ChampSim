#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"
#include "champsim_constants.h"

SCENARIO("An exclusive load causes an eviction") {
  GIVEN("A cache with a single filled entry") {
    constexpr uint64_t hit_latency = 4;
    constexpr uint64_t miss_latency = 3;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll{miss_latency};
    to_rq_MRP mock_ul_seed;
    to_rq_MRP mock_ul_test;
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l2c}
      .name("460-uut")
        .upper_levels({&mock_ul_seed.queues, &mock_ul_test.queues})
        .lower_level(&mock_ll.queues)
        .hit_latency(hit_latency)
        .fill_latency(fill_latency)
        .prefetch_activate(access_type::LOAD, access_type::RFO, access_type::PREFETCH, access_type::WRITE, access_type::TRANSLATION)
    };

    std::array<champsim::operable*, 4> elements{{&uut, &mock_ll, &mock_ul_seed, &mock_ul_test}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    // Create a test packet
    static uint64_t id = 1;
    decltype(mock_ul_seed)::request_type seed_a;
    seed_a.address = champsim::address{0xdeadbeef};
    seed_a.cpu = 0;
    seed_a.instr_id = id++;

    // Issue it to the uut
    auto seed_a_result = mock_ul_seed.issue(seed_a);
    THEN("This issue is received") {
      REQUIRE(seed_a_result);
    }

    // Run the uut for long enough to fulfill the request
    for (uint64_t i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("A exclusive load is issued") {
      decltype(mock_ul_test)::request_type seed_b = seed_a;
      seed_b.instr_id = id++;
      seed_b.clusivity = champsim::inclusivity::exclusive;

      // Issue it to the uut
      auto seed_b_result = mock_ul_test.issue(seed_b);
      THEN("This issue is received") {
        REQUIRE(seed_b_result);
      }

      // Run the uut for long enough to fulfill the request
      for (uint64_t i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      AND_WHEN("The first request is re-issued") {
        decltype(mock_ul_seed)::request_type test = seed_a;
        test.instr_id = id++;

        // Issue it to the uut
        auto test_result = mock_ul_seed.issue(test);
        THEN("This issue is received") {
          REQUIRE(test_result);
        }

        // Run the uut for long enough to fulfill the request
        for (uint64_t i = 0; i < 100; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The re-issue misses") {
          REQUIRE(mock_ul_seed.packets.back().return_time == mock_ul_seed.packets.back().issue_time + (fill_latency + miss_latency + hit_latency + 1)); // +1 due to ordering of elements
        }
      }
    }
  }
}

