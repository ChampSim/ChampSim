#include "catch.hpp"
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

extern bool warmup_complete[NUM_CPUS];

SCENARIO("A cache returns a hit after the specified latency") {
  GIVEN("A cache with one filled block") {
    constexpr uint64_t hit_latency = 7;
    do_nothing_MRC mock_ll;
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"401-uut", 1, 1, 8, 32, 3, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &mock_ll, CACHE::pref_t::pprefetcherDno, CACHE::repl_t::rreplacementDlru};
    to_rq_MRP mock_ul{&uut};

    std::array<champsim::operable*, 4> elements{{&mock_ll, &mock_ul, &uut_queues, &uut}};

    // Initialize the prefetching and replacement
    uut.impl_prefetcher_initialize();
    uut.impl_replacement_initialize();

    // Turn off warmup
    std::fill(std::begin(warmup_complete), std::end(warmup_complete), true);

    // Create a test packet
    static auto id = 1;
    PACKET seed;
    seed.address = 0xdeadbeef;
    seed.instr_id = id++;
    seed.cpu = 0;

    // Issue it to the uut
    auto seed_result = mock_ul.issue(seed);
    REQUIRE(seed_result);

    // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
    for (auto i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("A packet with the same address is sent") {
      auto test = seed;
      test.instr_id = id++;

      auto test_result = mock_ul.issue(test);
      REQUIRE(test_result);

      for (uint64_t i = 0; i < 2*hit_latency; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("It takes exactly the specified cycles to return") {
        REQUIRE(mock_ul.packets.back().return_time == mock_ul.packets.back().issue_time + hit_latency);
      }
    }
  }
}

