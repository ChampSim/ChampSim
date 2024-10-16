#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"

SCENARIO("Blocks that have been written are marked dirty") {
  GIVEN("An empty cache") {
    constexpr uint64_t hit_latency = 4;
    constexpr uint64_t miss_latency = 3;
    do_nothing_MRC mock_ll;
    to_wq_MRP mock_ul_seed;
    to_rq_MRP mock_ul_test;
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l2c}
      .name("407-uut")
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
      uint64_t id = 1;
      decltype(mock_ul_seed)::request_type seed_a;
      seed_a.address = champsim::address{0xdeadbeef};
      seed_a.cpu = 0;
      seed_a.type = access_type::WRITE;
      seed_a.instr_id = id++;

      auto seed_a_result = mock_ul_seed.issue(seed_a);

      for (uint64_t i = 0; i < 2*(miss_latency+hit_latency); ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The issue is received") {
        CHECK(seed_a_result);
        CHECK(mock_ll.packet_count() == 0);
      }

      AND_WHEN("The same packet is read") {
        auto seed_b = seed_a;
        seed_b.type = access_type::LOAD;
        seed_b.instr_id = id++;

        auto seed_b_result = mock_ul_test.issue(seed_b);

        for (uint64_t i = 0; i < 2*(miss_latency+hit_latency); ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The issue is received") {
          CHECK(seed_b_result);
          CHECK(mock_ll.packet_count() == 0);
        }

        AND_WHEN("A packet with a different address is sent") {
          decltype(mock_ul_test)::request_type test;
          test.address = champsim::address{0xcafebabe};
          test.cpu = 0;
          test.type = access_type::LOAD;
          test.instr_id = id++;

          auto test_b_result = mock_ul_test.issue(test);

          for (uint64_t i = 0; i < hit_latency+1; ++i)
            for (auto elem : elements)
              elem->_operate();

          THEN("The issue is received") {
            CHECK(test_b_result);
            REQUIRE_THAT(mock_ll.addresses, Catch::Matchers::RangeEquals(std::array{test.address}));
          }

          for (uint64_t i = 0; i < 2*(miss_latency+hit_latency); ++i)
            for (auto elem : elements)
              elem->_operate();

          THEN("It takes exactly the specified cycles to return") {
            REQUIRE_THAT(mock_ul_test.packets.back(), champsim::test::ReturnedMatcher(miss_latency + hit_latency + 1, 1));
          }

          THEN("The first block is evicted") {
            REQUIRE_THAT(mock_ll.addresses, Catch::Matchers::RangeEquals(std::array{test.address, seed_a.address}));
          }
        }
      }
    }
  }
}
