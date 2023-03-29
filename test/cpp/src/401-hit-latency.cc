#include <catch.hpp>
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

SCENARIO("A cache returns a hit after the specified latency") {
  using namespace std::literals;
  auto [type, str] = GENERATE(table<uint8_t, std::string_view>({
        std::pair{LOAD, "load"sv},
        std::pair{RFO, "RFO"sv},
        std::pair{PREFETCH, "prefetch"sv},
        std::pair{WRITE, "write"sv},
        std::pair{TRANSLATION, "translation"sv}
      }));

  GIVEN("An empty cache") {
    constexpr uint64_t hit_latency = 7;
    constexpr auto mask = ((1u<<LOAD) | (1u<<RFO) | (1u<<PREFETCH) | (1u<<WRITE) | (1u<<TRANSLATION)); // trigger prefetch on all types
    do_nothing_MRC mock_ll;
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"401-uut-"+std::string(str), 1, 1, 8, 32, 3, 1, 1, 0, false, false, false, mask, uut_queues, &mock_ll, CACHE::pprefetcherDno, CACHE::rreplacementDlru};
    to_rq_MRP mock_ul{&uut};

    std::array<champsim::operable*, 4> elements{{&mock_ll, &mock_ul, &uut_queues, &uut}};

    // Initialize the prefetching and replacement
    uut.initialize();

    // Turn off warmup
    uut.warmup = false;
    uut_queues.warmup = false;

    // Initialize stats
    uut.begin_phase();
    uut_queues.begin_phase();

    THEN("The number of hits starts at zero") {
      REQUIRE(uut.sim_stats.hits.at(type).at(0) == 0);
    }

    WHEN("A " + std::string{str} + " packet is issued") {
      // Create a test packet
      static uint64_t id = 1;
      PACKET seed;
      seed.address = champsim::address{0xdeadbeef};
      seed.instr_id = id++;
      seed.cpu = 0;
      seed.type = type;
      seed.to_return = {&mock_ul};

      // Issue it to the uut
      auto seed_result = mock_ul.issue(seed);
      THEN("This issue is received") {
        REQUIRE(seed_result);
      }

      // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      AND_WHEN("A packet with the same address is sent") {
        auto test = seed;
        test.instr_id = id++;

        auto test_result = mock_ul.issue(test);
        THEN("This issue is received") {
          REQUIRE(test_result);
        }

        for (uint64_t i = 0; i < 2*hit_latency; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("It takes exactly the specified cycles to return") {
          REQUIRE(mock_ul.packets.back().return_time == mock_ul.packets.back().issue_time + hit_latency);
        }

        THEN("The number of hits increases") {
          REQUIRE(uut.sim_stats.hits.at(type).at(0) == 1);
        }
      }
    }
  }
}

