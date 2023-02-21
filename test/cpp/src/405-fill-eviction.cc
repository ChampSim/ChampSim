#include <catch.hpp>
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

SCENARIO("A cache evicts a block when required") {
  GIVEN("An empty cache") {
    constexpr uint64_t hit_latency = 4;
    constexpr uint64_t miss_latency = 3;
    do_nothing_MRC mock_ll;
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"405-uut", 1, 1, 1, 32, miss_latency, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &mock_ll, CACHE::pprefetcherDno, CACHE::rreplacementDlru};
    to_wq_MRP mock_ul_seed{&uut};
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
      uint64_t id = 1;
      PACKET test_a;
      test_a.address = champsim::address{0xdeadbeef};
      test_a.cpu = 0;
      test_a.type = WRITE;
      test_a.instr_id = id++;

      auto test_a_result = mock_ul_seed.issue(test_a);

      for (uint64_t i = 0; i < 2*(miss_latency+hit_latency); ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The issue is received") {
        CHECK(test_a_result);
        CHECK(mock_ll.packet_count() == 0);
      }

      AND_WHEN("A packet with a different address is sent") {
        PACKET test_b;
        test_b.address = champsim::address{0xcafebabe};
        test_b.cpu = 0;
        test_b.type = LOAD;
        test_b.instr_id = id++;

        auto test_b_result = mock_ul_test.issue(test_b);

        for (uint64_t i = 0; i < hit_latency+1; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The issue is received") {
          CHECK(test_b_result);
          CHECK(mock_ll.packet_count() == 1);
          CHECK(mock_ll.addresses.back() == test_b.address);
        }

        for (uint64_t i = 0; i < 2*(miss_latency+hit_latency); ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("It takes exactly the specified cycles to return") {
          REQUIRE(mock_ul_test.packets.front().return_time == mock_ul_test.packets.front().issue_time + (miss_latency + hit_latency));
        }

        THEN("The first block is evicted") {
          CHECK(mock_ll.packet_count() == 2);
          CHECK(mock_ll.addresses.back() == test_a.address);
        }
      }
    }
  }
}


