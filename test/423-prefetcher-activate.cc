#include "catch.hpp"
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

#include <map>
#include <vector>

namespace test
{
  extern std::map<CACHE*, std::vector<uint32_t>> address_operate_collector;
}

SCENARIO("A prefetch does not trigger itself") {
  GIVEN("A single cache") {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll;
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"423-uut", 1, 1, 8, 32, fill_latency, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &mock_ll, CACHE::ptestDmodulesDprefetcherDaddress_collector, CACHE::rreplacementDlru};
    to_rq_MRP mock_ul{&uut};

    std::array<champsim::operable*, 4> elements{{&mock_ll, &mock_ul, &uut_queues, &uut}};

    // Initialize the prefetching and replacement
    uut.impl_prefetcher_initialize();
    uut.impl_replacement_initialize();

    // Turn off warmup
    for (auto elem : elements) {
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A prefetch is issued") {
      // Request a prefetch
      constexpr uint64_t seed_addr = 0xdeadbeef;
      auto seed_result = uut.prefetch_line(seed_addr, true, 0);
      CHECK(seed_result);

      // Run the uut for a bunch of cycles to clear it out of the PQ and fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The prefetcher is not called") {
        REQUIRE(std::empty(test::address_operate_collector[&uut]));
      }
    }
  }
}


