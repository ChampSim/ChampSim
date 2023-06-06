#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"
#include "champsim_constants.h"

SCENARIO("A cache returns a hit after the specified latency") {
  using namespace std::literals;
  auto [type, str] = GENERATE(table<access_type, std::string_view>({
        std::pair{access_type::LOAD, "load"sv},
        std::pair{access_type::RFO, "RFO"sv},
        std::pair{access_type::PREFETCH, "prefetch"sv},
        std::pair{access_type::WRITE, "write"sv},
        std::pair{access_type::TRANSLATION, "translation"sv}
      }));

  GIVEN("An empty cache") {
    constexpr uint64_t hit_latency = 7;
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{CACHE::Builder{champsim::defaults::default_l1d}
      .name("401-uut-"+std::string(str))
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .prefetch_activate(access_type::LOAD, access_type::RFO, access_type::PREFETCH, access_type::WRITE, access_type::TRANSLATION)
    };

    std::array<champsim::operable*, 3> elements{{&uut, &mock_ll, &mock_ul}};

    // Initialize the prefetching and replacement
    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    THEN("The number of hits starts at zero") {
      REQUIRE(uut.sim_stats.hits.at(champsim::to_underlying(type)).at(0) == 0);
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
          REQUIRE(uut.sim_stats.hits.at(champsim::to_underlying(type)).at(0) == 1);
        }
      }
    }
  }
}

