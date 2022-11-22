#include "catch.hpp"
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

SCENARIO("A cache returns a miss after the specified latency") {
  using namespace std::literals;
  auto [type, str] = GENERATE(table<uint8_t, std::string_view>({
        std::pair{LOAD, "load"sv},
        std::pair{RFO, "RFO"sv},
        std::pair{PREFETCH, "prefetch"sv},
        std::pair{WRITE, "write"sv},
        std::pair{TRANSLATION, "translation"sv}
      }));

  GIVEN("An empty cache") {
    constexpr uint64_t hit_latency = 4;
    constexpr uint64_t miss_latency = 3;
    constexpr auto mask = ((1u<<LOAD) | (1u<<RFO) | (1u<<PREFETCH) | (1u<<WRITE) | (1u<<TRANSLATION)); // trigger prefetch on all types
    do_nothing_MRC mock_ll;
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"402-uut-"+std::string(str), 1, 1, 8, 32, miss_latency, 1, 1, 0, false, false, false, mask, uut_queues, &mock_ll, CACHE::pprefetcherDno, CACHE::rreplacementDlru};
    to_rq_MRP mock_ul{&uut};

    std::array<champsim::operable*, 4> elements{{&mock_ll, &uut_queues, &uut, &mock_ul}};

    // Initialize the prefetching and replacement
    uut.impl_prefetcher_initialize();
    uut.impl_replacement_initialize();

    // Turn off warmup
    uut.warmup = false;
    uut_queues.warmup = false;
    uut.begin_phase();
    uut_queues.begin_phase();

    THEN("The number of misses starts at zero") {
      REQUIRE(uut.sim_stats.back().misses.at(type).at(0) == 0);
    }

    WHEN("A " + std::string{str} + " packet is issued") {
      // Create a test packet
      static uint64_t id = 1;
      PACKET test;
      test.address = 0xdeadbeef;
      test.cpu = 0;
      test.instr_id = id++;
      test.type = type;

      // Issue it to the uut
      auto test_result = mock_ul.issue(test);
      THEN("This issue is received") {
        REQUIRE(test_result);
      }

      // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
      for (uint64_t i = 0; i < 2*(miss_latency+hit_latency); ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("It takes exactly the specified cycles to return") {
        REQUIRE(mock_ul.packets.front().return_time == mock_ul.packets.front().issue_time + (miss_latency + hit_latency));
      }

      THEN("The number of misses increases") {
        REQUIRE(uut.sim_stats.front().misses.at(type).at(0) == 1);
      }
    }
  }
}

