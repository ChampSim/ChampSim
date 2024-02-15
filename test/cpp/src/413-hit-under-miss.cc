#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"

#include "cache.h"

TEMPLATE_TEST_CASE("Translation misses do not inhibit other packets from being issued", "", to_wq_MRP, to_rq_MRP, to_pq_MRP) {
  GIVEN("An empty cache") {
    constexpr static uint64_t hit_latency = 5;
    const champsim::address address_that_will_hit{0xcafebabe};
    filter_MRC mock_translator{address_that_will_hit};
    do_nothing_MRC mock_ll;
    TestType mock_ul{[](auto x, auto y){ return x.v_address == y.v_address; }};
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("413-uut")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .lower_translate(&mock_translator.queues)
      .hit_latency(hit_latency)
      .fill_latency(3)
    };

    std::array<champsim::operable*, 4> elements{{&uut, &mock_ll, &mock_translator, &mock_ul}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A packet is issued that will miss the translator") {
      // Create a test packet
      typename TestType::request_type seed;
      seed.address = champsim::address{0xdeadbeef};
      seed.v_address = champsim::address{0xdeadbeef};
      seed.is_translated = false;
      seed.cpu = 0;

      mock_ul.issue(seed);

      // Operate enough cycles to realize we've missed
      for (uint64_t i = 0; i < (hit_latency+1); ++i) {
        for (auto elem : elements)
          elem->_operate();
      }

      THEN("The packet has missed the translator") {
        REQUIRE(std::size(mock_ul.packets) == 1);
        REQUIRE(mock_ul.packets.front().return_time == 0);
      }

      AND_WHEN("An untranslated packet that will hit the translator is sent") {
        typename TestType::request_type test;
        test.address = address_that_will_hit;
        test.v_address = address_that_will_hit;
        seed.is_translated = false;
        test.cpu = 0;

        mock_ul.issue(test);

        // Operate long enough for the return to happen
        for (uint64_t i = 0; i < 100; ++i) {
          for (auto elem : elements)
            elem->_operate();
        }

        THEN("The second packet is issued") {
          REQUIRE(std::size(mock_ll.addresses) == 1);
          REQUIRE(mock_ll.addresses.front() == test.address);
        }
      }

      AND_WHEN("A translated packet is sent") {
        typename TestType::request_type test;
        test.address = champsim::address{0xfeedcafe};
        test.v_address = champsim::address{0xdeadbeef};
        test.is_translated = true;
        test.cpu = 0;

        mock_ul.issue(test);

        // Operate long enough for the return to happen
        for (uint64_t i = 0; i < 100; ++i) {
          for (auto elem : elements)
            elem->_operate();
        }

        THEN("The packet is translated") {
          REQUIRE(std::size(mock_ll.addresses) == 1);
          REQUIRE(mock_ll.addresses.front() == test.address);
        }
      }
    }
  }
}
