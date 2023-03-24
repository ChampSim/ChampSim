#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"
#include "champsim_constants.h"

SCENARIO("A cache returns a miss after the specified latency") {
  using namespace std::literals;
  auto [type, str] = GENERATE(table<uint8_t, std::string_view>({
        std::pair{LOAD, "load"sv},
        std::pair{RFO, "RFO"sv},
        std::pair{PREFETCH, "prefetch"sv},
        std::pair{TRANSLATION, "translation"sv}
      }));

  GIVEN("An empty cache") {
    constexpr uint64_t hit_latency = 4;
    constexpr uint64_t miss_latency = 3;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll{miss_latency};
    to_rq_MRP mock_ul;
    CACHE uut{CACHE::Builder{champsim::defaults::default_l1d}
      .name("402-uut-"+std::string(str))
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .prefetch_activate(LOAD, RFO, PREFETCH, WRITE, TRANSLATION)
    };

    std::array<champsim::operable*, 3> elements{{&uut, &mock_ll, &mock_ul}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    THEN("The number of misses starts at zero") {
      REQUIRE(uut.sim_stats.misses.at(type).at(0) == 0);
    }

    WHEN("A " + std::string{str} + " packet is issued") {
      // Create a test packet
      static uint64_t id = 1;
      decltype(mock_ul)::request_type test;
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
      for (uint64_t i = 0; i < 2*(miss_latency+hit_latency+fill_latency); ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("It takes exactly the specified cycles to return") {
        REQUIRE(mock_ul.packets.front().return_time == mock_ul.packets.front().issue_time + (fill_latency + miss_latency + hit_latency + 1)); // +1 due to ordering of elements
      }

      THEN("The number of misses increases") {
        REQUIRE(uut.sim_stats.misses.at(type).at(0) == 1);
      }

      THEN("The average miss latency increases") {
        REQUIRE(uut.sim_stats.total_miss_latency == miss_latency + fill_latency);
      }
    }
  }
}

SCENARIO("A cache completes a fill after the specified latency") {
  using namespace std::literals;
  auto [type, str] = std::pair{WRITE, "write"sv};
  auto match_offset = GENERATE(true, false);

  GIVEN("An empty cache") {
    constexpr uint64_t hit_latency = 4;
    constexpr uint64_t miss_latency = 3;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll{miss_latency};
    to_wq_MRP mock_ul;
    auto builder = CACHE::Builder{champsim::defaults::default_l1d}
      .name("402-uut-"+std::string(str))
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .prefetch_activate(LOAD, RFO, PREFETCH, WRITE, TRANSLATION);

    if (match_offset)
      builder = builder.set_wq_checks_full_addr();
    else
      builder = builder.reset_wq_checks_full_addr();

    CACHE uut{builder};

    std::array<champsim::operable*, 3> elements{{&uut, &mock_ll, &mock_ul}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    THEN("The number of misses starts at zero") {
      REQUIRE(uut.sim_stats.misses.at(type).at(0) == 0);
    }

    WHEN("A " + std::string{str} + " packet is issued") {
      // Create a test packet
      static uint64_t id = 1;
      decltype(mock_ul)::request_type test;
      test.address = 0xdeadbeef;
      test.cpu = 0;
      test.instr_id = id++;
      test.type = type;

      // Issue it to the uut
      auto test_result = mock_ul.issue(test);
      THEN("This issue is received") {
        REQUIRE(test_result);
      }

      // Run the uut for a bunch of cycles to clear it out of the WQ and fill the cache
      for (uint64_t i = 0; i < 2*(miss_latency+hit_latency+fill_latency); ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("It takes exactly the specified cycles to return") {
        if (match_offset)
          REQUIRE(mock_ul.packets.front().return_time == mock_ul.packets.front().issue_time + (fill_latency + miss_latency + hit_latency + 1)); // +1 due to ordering of elements
        else
          REQUIRE(mock_ul.packets.front().return_time == mock_ul.packets.front().issue_time + (fill_latency + hit_latency));
      }

      THEN("The number of misses increases") {
        REQUIRE(uut.sim_stats.misses.at(type).at(0) == 1);
      }

      THEN("The average miss latency increases") {
        if (match_offset)
          REQUIRE(uut.sim_stats.total_miss_latency == miss_latency + fill_latency);
        else
          REQUIRE(uut.sim_stats.total_miss_latency == fill_latency-1); // -1 due to ordering of elements
      }
    }
  }
}
