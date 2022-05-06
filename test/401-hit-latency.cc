#include "catch.hpp"
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

#include "dram_controller.h"
#include "vmem.h"

MEMORY_CONTROLLER dram{1};
VirtualMemory vmem{20, 1 << 12, 5, 200, dram};

SCENARIO("A cache returns a hit after the specified latency") {
  GIVEN("A cache with one filled block") {
    constexpr uint64_t hit_latency = 4;
    do_nothing_MRC mock_ll;
    CACHE uut{"uut", 1, 1, 1, 8, 32, 32, 32, 32, hit_latency, 3, 1, 1, LOG2_BLOCK_SIZE, 0, 0, 0, 5, &mock_ll, (1 << CACHE::pprefetcherDno), (1 << CACHE::rreplacementDlru)};
    to_rq_MRP mock_ul{&uut};

    std::array<champsim::operable*, 3> elements{{&mock_ll, &uut, &mock_ul}};

    // Initialize the prefetching and replacement
    uut.impl_prefetcher_initialize();
    uut.impl_replacement_initialize();

    // Turn off warmup
    uut.warmup = false;

    // Initialize stats
    uut.begin_phase();

    // Create a test packet
    PACKET seed;
    seed.address = 0xdeadbeef;
    seed.cpu = 0;
    seed.to_return = {&mock_ul};

    // Issue it to the uut
    auto seed_result = mock_ul.issue(seed);
    REQUIRE(seed_result);

    // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
    for (auto i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("A packet with the same address is sent") {
      auto test = seed;

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

