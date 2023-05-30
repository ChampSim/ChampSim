#include <catch.hpp>
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"
#include "defaults.hpp"

#include <cstdlib>

SCENARIO("A cache increments the useless prefetch count when it evicts an unhit prefetch") {
  GIVEN("An empty cache") {
    constexpr uint64_t hit_latency = 4;
    constexpr uint64_t miss_latency = 3;
    do_nothing_MRC mock_ll;
    to_wq_MRP mock_ul_seed;
    to_rq_MRP mock_ul_test;
    CACHE uut{CACHE::Builder{champsim::defaults::default_l2c}
      .name("405-uut")
      .sets(1)
      .ways(1)
      .upper_levels({{&mock_ul_seed.queues, &mock_ul_test.queues}})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(miss_latency)
    };

    std::array<champsim::operable*, 4> elements{{&uut, &mock_ll, &mock_ul_seed, &mock_ul_test}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    // Run the uut for a few cycles
    for (auto i = 0; i < 10; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("A packet is sent") {
      constexpr uint64_t seed_addr = 0xdeadbeef;
      auto seed_result = uut.prefetch_line(seed_addr, true, 0);

      for (uint64_t i = 0; i < 2*(miss_latency+hit_latency); ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The issue is received") {
        CHECK(seed_result);
        CHECK(mock_ll.packet_count() == 1);
      }

      AND_WHEN("A packet with a different address is sent") {
        decltype(mock_ul_test)::request_type test_b;
        test_b.address = 0xcafebabe;
        test_b.cpu = 0;
        test_b.type = access_type::LOAD;
        test_b.instr_id = 1;

        auto test_b_result = mock_ul_test.issue(test_b);

        for (uint64_t i = 0; i < hit_latency+2; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The issue is received") {
          CHECK(test_b_result);
          CHECK(mock_ll.packet_count() == 2);
          CHECK(mock_ll.addresses.back() == test_b.address);
        }

        for (uint64_t i = 0; i < 2*(miss_latency+hit_latency); ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("It takes exactly the specified cycles to return") {
          REQUIRE(std::llabs((long long)mock_ul_test.packets.front().return_time - ((long long)mock_ul_test.packets.front().issue_time + (long long)(miss_latency + hit_latency))) <= 1);
        }

        THEN("The number of useless prefetches is increased") {
          REQUIRE(uut.sim_stats.pf_useless == 1);
        }
      }
    }
  }
}

