#include "catch.hpp"
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

SCENARIO("The Berti prefetcher issues prefetches when the IP matches") {
  GIVEN("A cache with zero filled block") {
    do_nothing_MRC mock_ll;
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, 1, LOG2_BLOCK_SIZE, false};
    CACHE uut{"453-uut-[berti]", 1, 1, 8, 32, 3, 1, 1, 0, false, false, true, (1<<LOAD)|(1<<PREFETCH), uut_queues, &mock_ll, CACHE::pprefetcherDberti, CACHE::rreplacementDlru};
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

    static uint64_t id = 1;
    PACKET seed;
    seed.address = 0xffff'003f;
    seed.ip = 0xcafecafe;
    seed.instr_id = id++;
    seed.cpu = 0;
    seed.to_return = {&mock_ul};

    // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
    for (auto i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("20 more packets with the same IP but strided address is sent") {
      for (int i = 0; i < 20; i++)
      {
        auto test_a = seed;
        test_a.address = static_cast<uint64_t>(seed.address + i*BLOCK_SIZE);
        test_a.instr_id = id++;

        auto test_result_a = mock_ul.issue(test_a);
        THEN("The issue is accepted") {
          REQUIRE(test_result_a);
        }
      }

      for (uint64_t i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("More than 20 requests were generated") {
        REQUIRE(mock_ll.packet_count() >= 20);
      }

      THEN("All of the issued requests have the same stride") {
        int stride = 1;
        for (unsigned i = 0; i < mock_ll.packet_count()-1; i++)
        {
          REQUIRE((mock_ll.addresses.at(i) >> LOG2_BLOCK_SIZE) + stride == (mock_ll.addresses.at(i+1) >> LOG2_BLOCK_SIZE));
        }
      }
    }
  }
}
