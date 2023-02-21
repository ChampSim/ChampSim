#include <catch.hpp>
#include "mocks.hpp"
#include "channel.h"
#include "champsim_constants.h"

TEMPLATE_TEST_CASE("Caches issue translations", "", to_wq_MRP, to_rq_MRP, to_pq_MRP) {
  GIVEN("An empty cache with a translator") {
    constexpr uint64_t hit_latency = 10;
    do_nothing_MRC mock_translator;
    do_nothing_MRC mock_ll;
    TestType mock_ul{[](auto x, auto y){ return x.v_address == y.v_address; }};
    CACHE uut{"411a-uut", 1, 1, 8, 32, hit_latency, 3, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), {&mock_ul.queues}, &mock_translator.queues, &mock_ll.queues, CACHE::pprefetcherDno, CACHE::rreplacementDlru};

    std::array<champsim::operable*, 4> elements{{&uut, &mock_ll, &mock_ul, &mock_translator}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A packet is sent") {
      // Create a test packet
      typename TestType::request_type test;
      test.address = 0xdeadbeef;
      test.v_address = 0xdeadbeef;
      test.is_translated = false;
      test.cpu = 0;

      auto test_result = mock_ul.issue(test);
      THEN("The issue is accepted") {
        REQUIRE(test_result);
      }

      for (auto elem : elements)
        elem->_operate();

      THEN("The packet is issued for translation") {
        REQUIRE(mock_translator.packet_count() == 1);
      }

      for (int i = 0; i < 100; ++i) {
        for (auto elem : elements)
          elem->_operate();
      }

      THEN("The packet is translated") {
        REQUIRE(std::size(mock_ll.addresses) == 1);
        REQUIRE(mock_ll.addresses.front() == 0x11111eef);
      }
    }
  }
}

TEMPLATE_TEST_CASE("Translations work even if the addresses happen to be the same", "", to_wq_MRP, to_rq_MRP, to_pq_MRP) {
  GIVEN("An empty cache with a translator") {
    constexpr uint64_t hit_latency = 10;
    release_MRC mock_translator; // release_MRC used because it does not manipulate the data field
    do_nothing_MRC mock_ll;
    TestType mock_ul{[](auto x, auto y){ return x.v_address == y.v_address; }};
    CACHE uut{"411a-uut", 1, 1, 8, 32, hit_latency, 3, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), {&mock_ul.queues}, &mock_translator.queues, &mock_ll.queues, CACHE::pprefetcherDno, CACHE::rreplacementDlru};

    std::array<champsim::operable*, 4> elements{{&uut, &mock_ll, &mock_ul, &mock_translator}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A packet is sent") {
      // Create a test packet
      typename TestType::request_type test;
      test.address = 0xdeadbeef;
      test.v_address = test.address;
      test.is_translated = false;
      test.data = test.address; // smuggle our own translation through the mock
      test.cpu = 0;

      auto test_result = mock_ul.issue(test);
      THEN("The issue is accepted") {
        REQUIRE(test_result);
      }

      for (auto elem : elements)
        elem->_operate();

      THEN("The packet is issued for translation") {
        REQUIRE(mock_translator.packet_count() == 1);
      }

      mock_translator.release(test.address);
      for (int i = 0; i < 100; ++i) {
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

