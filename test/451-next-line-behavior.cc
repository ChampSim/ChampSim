#include "catch.hpp"
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

SCENARIO("The next line prefetcher issues prefetches") {
  GIVEN("An empty cache") {
    do_nothing_MRC mock_ll;
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, 1, LOG2_BLOCK_SIZE, false};
    CACHE uut{"451-uut", 1, 1, 8, 32, 3, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &mock_ll, CACHE::pprefetcherDnext_line, CACHE::rreplacementDlru};
    to_rq_MRP mock_ul{&uut};

    std::array<champsim::operable*, 4> elements{{&mock_ll, &mock_ul, &uut_queues, &uut}};

    // Initialize the prefetching and replacement
    uut.impl_prefetcher_initialize();
    uut.impl_replacement_initialize();

    // Turn off warmup
    uut.warmup = false;
    uut_queues.warmup = false;

    // Initialize stats
    uut.begin_phase();
    uut_queues.begin_phase();

    WHEN("A packet is issued") {
      // Create a test packet
      static uint64_t id = 1;
      PACKET seed;
      seed.address = champsim::address{0xffff'003f};
      seed.instr_id = id++;
      seed.cpu = 0;
      seed.to_return = {&mock_ul};

      // Issue it to the uut
      auto seed_result = mock_ul.issue(seed);
      THEN("The issue is accepted") {
        REQUIRE(seed_result);
      }

      // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("A total of 2 requests were generated") {
        REQUIRE(mock_ll.packet_count() == 2);
      }

      THEN("All of the issued requests have the same stride") {
        REQUIRE(champsim::block_number{mock_ll.addresses.at(0)} + 1 == champsim::block_number{mock_ll.addresses.at(1)});
      }
    }
  }
}



