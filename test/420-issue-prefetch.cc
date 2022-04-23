#include "catch.hpp"
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

SCENARIO("A prefetch can be issued") {
  GIVEN("One issued prefetch") {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll;
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"420-uut", 1, 1, 8, 32, fill_latency, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &mock_ll, CACHE::pref_t::pprefetcherDno, CACHE::repl_t::rreplacementDlru};
    to_rq_MRP mock_ul{&uut};

    std::array<champsim::operable*, 4> elements{{&mock_ll, &mock_ul, &uut_queues, &uut}};

    // Initialize the prefetching and replacement
    uut.impl_prefetcher_initialize();
    uut.impl_replacement_initialize();

    // Turn off warmup
    uut.warmup = false;
    uut_queues.warmup = false;
    uut.begin_phase();
    uut_queues.begin_phase();

    // Request a prefetch
    constexpr uint64_t seed_addr = 0xdeadbeef;
    auto seed_result = uut.prefetch_line(seed_addr, true, 0);
    REQUIRE(seed_result);

    // Run the uut for a bunch of cycles to clear it out of the PQ and fill the cache
    for (auto i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("A packet with the same address is sent") {
      // Create a test packet
      PACKET test;
      test.address = 0xdeadbeef;
      test.cpu = 0;

      auto test_result = mock_ul.issue(test);
      REQUIRE(test_result);

      for (uint64_t i = 0; i < 2*hit_latency; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The packet hits the cache") {
        REQUIRE(mock_ul.packets.back().return_time == mock_ul.packets.back().issue_time + hit_latency);
      }
    }
  }
}

