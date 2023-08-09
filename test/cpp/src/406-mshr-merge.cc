#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"
#include "champsim_constants.h"

#include <map>

namespace test
{
  extern std::map<CACHE*, std::vector<uint64_t>> address_operate_collector;
}

SCENARIO("A cache merges two requests in the MSHR") {
  GIVEN("An empty cache") {
    constexpr uint64_t hit_latency = 4;
    constexpr uint64_t fill_latency = 10;
    constexpr uint64_t miss_latency = 2;
    do_nothing_MRC mock_ll{miss_latency};
    to_rq_MRP mock_ul_seed;
    to_rq_MRP mock_ul_test;
    CACHE uut{CACHE::Builder{champsim::defaults::default_l1d}
      .name("406-uut")
      .upper_levels({{&mock_ul_seed.queues, &mock_ul_test.queues}})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .prefetcher<CACHE::ptestDcppDmodulesDprefetcherDaddress_collector>()
    };

    std::array<champsim::operable*, 4> elements{{&mock_ll, &uut, &mock_ul_seed, &mock_ul_test}};

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
      test::address_operate_collector[&uut].clear();

      uint64_t id = 1;
      decltype(mock_ul_seed)::request_type test_a;
      test_a.address = 0xdeadbeef;
      test_a.cpu = 0;
      test_a.type = access_type::LOAD;
      test_a.instr_id = id++;

      auto test_a_result = mock_ul_seed.issue(test_a);

      for (uint64_t i = 0; i < hit_latency+2; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The issue is received") {
        CHECK(test_a_result);
        CHECK(mock_ll.packet_count() == 1);
      }

      THEN("The prefetcher is called") {
        REQUIRE(std::size(test::address_operate_collector[&uut]) == 1);
      }

      AND_WHEN("A packet with the same address is sent before the fill has completed") {
        test::address_operate_collector[&uut].clear();

        decltype(mock_ul_test)::request_type test_b = test_a;
        test_b.instr_id = id++;

        auto test_b_result = mock_ul_test.issue(test_b);

        for (uint64_t i = 0; i < 100; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The issue is received") {
          REQUIRE(test_b_result);
        }

        THEN("The prefetcher is called") {
          REQUIRE(std::size(test::address_operate_collector[&uut]) == 1);
        }

        THEN("The test packet was not forwarded to the lower level") {
          REQUIRE(mock_ll.packet_count() == 1);
        }

        THEN("The upper level for the test packet received its return without delay") {
          REQUIRE(std::size(mock_ul_test.packets) == 1);
          REQUIRE(std::size(mock_ul_seed.packets) == 1);
          REQUIRE(mock_ul_test.packets.front().return_time == mock_ul_seed.packets.front().issue_time + (fill_latency + miss_latency + hit_latency + 1)); // +1 due to ordering of elements
        }
      }
    }
  }
}



