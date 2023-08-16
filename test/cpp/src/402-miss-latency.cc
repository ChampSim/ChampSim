#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"
#include "champsim_constants.h"

SCENARIO("A cache returns a miss after the specified latency") {
  using namespace std::literals;
  auto [type, str] = GENERATE(table<access_type, std::string_view>({
        std::pair{access_type::LOAD, "load"sv},
        std::pair{access_type::RFO, "RFO"sv},
        std::pair{access_type::PREFETCH, "prefetch"sv},
        std::pair{access_type::TRANSLATION, "translation"sv}
      }));

  GIVEN("An empty cache") {
    constexpr uint64_t hit_latency = 4;
    constexpr uint64_t miss_latency = 3;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll{miss_latency};
    to_rq_MRP mock_ul;
    CACHE uut{CACHE::Builder{champsim::defaults::default_l1d}
      .name("402a-uut-"+std::string(str))
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .prefetch_activate(access_type::LOAD, access_type::RFO, access_type::PREFETCH, access_type::WRITE, access_type::TRANSLATION)
    };

    std::array<champsim::operable*, 3> elements{{&uut, &mock_ll, &mock_ul}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    THEN("The number of misses starts at zero") {
      REQUIRE(uut.sim_stats.misses.at(champsim::to_underlying(type)).at(0) == 0);
    }

    THEN("The MSHR occupancy starts at zero") {
      CHECK(uut.get_mshr_occupancy() == 0);
      CHECK(uut.get_mshr_occupancy_ratio() == 0);
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

      // Run the uut for long enough to miss
      for (uint64_t i = 0; i < hit_latency+1; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The MSHR occupancy increases") {
        CHECK(uut.get_mshr_occupancy() == 1);
        CHECK(uut.get_mshr_occupancy_ratio() > 0);
        CHECK(uut.get_mshr_occupancy_ratio() == (std::ceil(uut.get_mshr_occupancy()) / std::ceil(uut.get_mshr_size())));
      }

      // Run the uut for long enough to fill the cache
      for (uint64_t i = 0; i < 2*(miss_latency+fill_latency); ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("It takes exactly the specified cycles to return") {
        REQUIRE(mock_ul.packets.front().return_time == mock_ul.packets.front().issue_time + (fill_latency + miss_latency + hit_latency + 1)); // +1 due to ordering of elements
      }

      THEN("The number of misses increases") {
        REQUIRE(uut.sim_stats.misses.at(champsim::to_underlying(type)).at(0) == 1);
      }

      THEN("The average miss latency increases") {
        REQUIRE(uut.sim_stats.total_miss_latency == miss_latency + fill_latency);
      }

      THEN("The end-of-phase average miss latency increases") {
        uut.end_phase(0);
        REQUIRE(uut.sim_stats.avg_miss_latency == miss_latency + fill_latency);
        REQUIRE(uut.roi_stats.avg_miss_latency == miss_latency + fill_latency);
      }
    }
  }
}

SCENARIO("A cache completes a fill after the specified latency") {
  using namespace std::literals;
  auto [type, str] = std::pair{access_type::WRITE, "write"sv};
  auto match_offset = GENERATE(true, false);

  GIVEN("An empty cache") {
    constexpr uint64_t hit_latency = 4;
    constexpr uint64_t miss_latency = 3;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll{miss_latency};
    to_wq_MRP mock_ul;
    auto builder = CACHE::Builder{champsim::defaults::default_l1d}
      .name("402b-uut-"+std::string(str))
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .prefetch_activate(access_type::LOAD, access_type::RFO, access_type::PREFETCH, access_type::WRITE, access_type::TRANSLATION);

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
      REQUIRE(uut.sim_stats.misses.at(champsim::to_underlying(type)).at(0) == 0);
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
        REQUIRE(uut.sim_stats.misses.at(champsim::to_underlying(type)).at(0) == 1);
      }

      THEN("The average miss latency increases") {
        if (match_offset)
          REQUIRE(uut.sim_stats.total_miss_latency == miss_latency + fill_latency);
        else
          REQUIRE(uut.sim_stats.total_miss_latency == fill_latency-1); // -1 due to ordering of elements
      }

      THEN("The end-of-phase average miss latency increases") {
        uut.end_phase(0);
        if (match_offset) {
          REQUIRE(uut.sim_stats.avg_miss_latency == miss_latency + fill_latency);
          REQUIRE(uut.roi_stats.avg_miss_latency == miss_latency + fill_latency);
        } else {
          REQUIRE(uut.sim_stats.avg_miss_latency == fill_latency-1); // -1 due to ordering of elements
          REQUIRE(uut.roi_stats.avg_miss_latency == fill_latency-1); // -1 due to ordering of elements
        }
      }
    }
  }
}

SCENARIO("The MSHR bandwidth limits the number of outstanding misses") {
  GIVEN("An cache with full MSHRs") {
    release_MRC mock_ll;
    to_rq_MRP mock_ul_seed;
    to_rq_MRP mock_ul_test;
    CACHE uut{CACHE::Builder{champsim::defaults::default_l1d}
      .name("402c-uut")
        .upper_levels({{&mock_ul_seed.queues, &mock_ul_test.queues}})
        .lower_level(&mock_ll.queues)
        .mshr_size(1)
    };

    std::array<champsim::operable*, 4> elements{{&mock_ll, &uut, &mock_ul_seed, &mock_ul_test}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    uint64_t id = 1;
    decltype(mock_ul_seed)::request_type test_a;
    test_a.address = 0xdeadbeef;
    test_a.cpu = 0;
    test_a.type = access_type::LOAD;
    test_a.instr_id = id++;

    auto test_a_result = mock_ul_seed.issue(test_a);

    for (uint64_t i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    THEN("The issue is received") {
      CHECK(test_a_result);
      CHECK(mock_ll.packet_count() == 1);
    }

    WHEN("A packet with a different address is sent before the fill has completed") {
      decltype(mock_ul_test)::request_type test_b;
      test_b.address = 0xcafebabe;
      test_b.cpu = 0;
      test_b.type = access_type::LOAD;
      test_b.instr_id = id++;

      auto test_b_result = mock_ul_test.issue(test_b);

      THEN("The issue is received") {
        REQUIRE(test_b_result);
      }

      uint64_t first_packet_delay = 10;
      for (uint64_t i = 0; i < first_packet_delay; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The test packet was not forwarded to the lower level") {
        REQUIRE(mock_ll.packet_count() == 1);
      }

      THEN("The number of misses did not increase") {
        REQUIRE(uut.sim_stats.misses.at(champsim::to_underlying(access_type::LOAD)).at(0) == 1);
      }
    }
  }
}

SCENARIO("A lower-level queue refusal limits the number of outstanding misses") {
  GIVEN("An empty cache") {
    champsim::channel refusal_channel{0,0,0,0,0}; // Refuses all packets
    to_rq_MRP mock_ul;
    CACHE uut{CACHE::Builder{champsim::defaults::default_l1d}
      .name("402c-uut")
      .upper_levels({{&mock_ul.queues}})
      .lower_level(&refusal_channel)
    };

    std::array<champsim::operable*, 2> elements{{&uut, &mock_ul}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A packet is sent") {
      uint64_t id = 1;
      decltype(mock_ul)::request_type test_a;
      test_a.address = 0xdeadbeef;
      test_a.cpu = 0;
      test_a.type = access_type::LOAD;
      test_a.instr_id = id++;

      auto test_a_result = mock_ul.issue(test_a);

      for (uint64_t i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The issue is received") {
        CHECK(test_a_result);
      }

      uint64_t first_packet_delay = 10;
      for (uint64_t i = 0; i < first_packet_delay; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The number of misses did not increase") {
        REQUIRE(uut.sim_stats.misses.at(champsim::to_underlying(access_type::LOAD)).at(0) == 0);
      }
    }
  }
}
