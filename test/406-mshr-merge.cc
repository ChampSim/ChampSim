#include "catch.hpp"
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

#include <map>

namespace test
{
  extern std::map<CACHE*, std::vector<champsim::address>> address_operate_collector;
}

SCENARIO("A cache merges two requests in the MSHR") {
  GIVEN("An empty cache") {
    constexpr uint64_t hit_latency = 4;
    release_MRC mock_ll;
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"406-uut", 1, 8, 8, 32, 1, 2, 2, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &mock_ll, CACHE::ptestDmodulesDprefetcherDaddress_collector, CACHE::rreplacementDlru};
    to_rq_MRP mock_ul_seed{&uut};
    to_rq_MRP mock_ul_test{&uut};

    std::array<champsim::operable*, 5> elements{{&mock_ll, &uut_queues, &uut, &mock_ul_seed, &mock_ul_test}};

    // Initialize the prefetching and replacement
    uut.initialize();

    // Turn off warmup
    uut.warmup = false;
    uut_queues.warmup = false;
    uut.begin_phase();
    uut_queues.begin_phase();

    // Run the uut for a few cycles
    for (auto i = 0; i < 10; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("A packet is sent") {
      test::address_operate_collector.insert_or_assign(&uut, std::vector<champsim::address>{});

      uint64_t id = 1;
      PACKET test_a;
      test_a.address = champsim::address{0xdeadbeef};
      test_a.cpu = 0;
      test_a.type = LOAD;
      test_a.instr_id = id++;

      auto test_a_result = mock_ul_seed.issue(test_a);

      for (uint64_t i = 0; i < hit_latency+1; ++i)
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
        test::address_operate_collector.insert_or_assign(&uut, std::vector<champsim::address>{});

        PACKET test_b = test_a;
        test_b.instr_id = id++;

        auto test_b_result = mock_ul_test.issue(test_b);

        for (uint64_t i = 0; i < 10; ++i)
          for (auto elem : elements)
            elem->_operate();

        mock_ll.release(test_a.address);

        for (uint64_t i = 0; i < 10; ++i)
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

        THEN("The upper level for the test packet received its return") {
          REQUIRE(std::size(mock_ul_test.packets) == 1);
          REQUIRE(mock_ul_test.packets.front().return_time != 0);
        }
      }
    }
  }
}



