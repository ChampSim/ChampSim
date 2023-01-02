#include "catch.hpp"
#include "mocks.hpp"

#include "cache.h"
#include "champsim_constants.h"

TEMPLATE_TEST_CASE("Translation misses do not inhibit other packets from being issued", "", to_wq_MRP<CACHE>, to_rq_MRP<CACHE>, to_pq_MRP<CACHE>) {
  GIVEN("An empty cache") {
    constexpr static uint64_t hit_latency = 5;
    constexpr static uint64_t address_that_will_hit = 0xcafebabe;
    filter_MRC mock_translator{address_that_will_hit};
    do_nothing_MRC mock_ll;
    champsim::channel uut_queues{1, 32, 32, 32, 0, LOG2_BLOCK_SIZE, false};
    CACHE uut{"413-uut", 1, 1, 8, 32, hit_latency, 3, 1, 1, 0, false, true, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &mock_translator, &mock_ll, CACHE::pprefetcherDno, CACHE::rreplacementDlru};
    TestType mock_ul{&uut, [](PACKET x, PACKET y){ return x.v_address == y.v_address; }};

    std::array<champsim::operable*, 5> elements{{&uut, &uut_queues, &mock_ll, &mock_translator, &mock_ul}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A packet is issued that will miss the translator") {
      // Create a test packet
      PACKET seed;
      seed.address = 0xdeadbeef;
      seed.v_address = 0xdeadbeef;
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
        PACKET test;
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
        PACKET test;
        test.address = 0xfeedcafe;
        test.v_address = 0xdeadbeef;
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
