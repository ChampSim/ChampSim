#include <catch.hpp>
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

SCENARIO("A cache returns a hit after the specified latency") {
  using namespace std::literals;
  auto [type, str] = GENERATE(table<access_type, std::string_view>({
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
    to_rq_MRP mock_ul;
    CACHE uut{"401-uut-"+std::string(str), 1, 1, 8, 32, hit_latency, 3, 1, 1, 0, false, false, false, mask, {&mock_ul.queues}, nullptr, &mock_ll.queues, CACHE::pprefetcherDno, CACHE::rreplacementDlru};

    std::array<champsim::operable*, 3> elements{{&uut, &mock_ll, &mock_ul}};

    // Initialize the prefetching and replacement
    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    THEN("The number of hits starts at zero") {
      REQUIRE(uut.sim_stats.hits.at(type).at(0) == 0);
    }

    WHEN("A " + std::string{str} + " packet is issued") {
      // Create a test packet
      static uint64_t id = 1;
      decltype(mock_ul)::request_type seed;
      seed.address = 0xdeadbeef;
      seed.is_translated = true;
      seed.instr_id = id++;
      seed.cpu = 0;
      seed.type = type;

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
          REQUIRE(std::size(mock_ul.packets) == 2);
          REQUIRE(mock_ul.packets.back().return_time == mock_ul.packets.back().issue_time + hit_latency);
        }

        THEN("The number of hits increases") {
          REQUIRE(uut.sim_stats.hits.at(type).at(0) == 1);
        }
      }
    }
  }
}

