#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "channel.h"
#include "champsim_constants.h"

TEMPLATE_TEST_CASE("Caches detect translation misses", "", to_wq_MRP, to_rq_MRP, to_pq_MRP) {
  GIVEN("An empty cache with a translator") {
    constexpr uint64_t hit_latency = 10;
    constexpr uint64_t fill_latency = 3;
    do_nothing_MRC mock_translator{2*hit_latency};
    do_nothing_MRC mock_ll;
    TestType mock_ul{[](auto x, auto y){ return x.v_address == y.v_address; }};
    CACHE uut{CACHE::Builder{champsim::defaults::default_l1d}
      .name("412a-uut")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .lower_translate(&mock_translator.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
    };

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
        REQUIRE(mock_ul.packets.front().pkt.v_address == test.v_address);
      }

      THEN("The packet restarted the tag lookup") {
        REQUIRE(std::size(mock_ll.addresses) == 1);
        REQUIRE(mock_ul.packets.front().return_time == mock_ul.packets.front().issue_time + 3*hit_latency+fill_latency+2); // latency = translator_time + hit_latency + fill_latency + 2 (clocking delay)
      }
    }
  }
}

