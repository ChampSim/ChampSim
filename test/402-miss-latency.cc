#include "catch.hpp"
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

extern bool warmup_complete[NUM_CPUS];

SCENARIO("A cache returns a miss after the specified latency") {
  GIVEN("An empty cache") {
    constexpr uint64_t hit_latency = 4;
    constexpr uint64_t miss_latency = 3;
    do_nothing_MRC mock_ll;
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"402-uut", 1, 1, 8, 32, miss_latency, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &mock_ll, CACHE::pref_t::pprefetcherDno, CACHE::repl_t::rreplacementDlru};
    to_rq_MRP mock_ul{&uut};

    std::array<champsim::operable*, 4> elements{{&mock_ll, &uut_queues, &uut, &mock_ul}};

    // Initialize the prefetching and replacement
    uut.impl_prefetcher_initialize();
    uut.impl_replacement_initialize();

    // Turn off warmup
    std::fill(std::begin(warmup_complete), std::end(warmup_complete), true);

    // Run the uut for a few cycles
    for (auto i = 0; i < 10; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("A packet is sent") {

    // Create a test packet
      static auto id = 1;
      PACKET test;
      test.address = 0xdeadbeef;
      test.cpu = 0;
      test.instr_id = id++;

      auto test_result = mock_ul.issue(test);
      REQUIRE(test_result);

      for (uint64_t i = 0; i < 2*(miss_latency+hit_latency); ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("It takes exactly the specified cycles to return") {
        REQUIRE(mock_ul.packets.front().return_time == mock_ul.packets.front().issue_time + (miss_latency + hit_latency));
      }
    }
  }
}

