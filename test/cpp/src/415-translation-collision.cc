#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "channel.h"

SCENARIO("A cache keeps the address for packets that don't need translation") {
  GIVEN("An empty cache with one inflight translation") {
    constexpr uint64_t hit_latency = 10;
    do_nothing_MRC mock_translator;
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul{[](auto x, auto y){ return x.v_address == y.v_address; }};
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("415-uut")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .lower_translate(&mock_translator.queues)
      .hit_latency(hit_latency)
      .fill_latency(3)
      .tag_bandwidth(champsim::bandwidth::maximum_type{10})
    };

    std::array<champsim::operable*, 4> elements{{&uut, &mock_ll, &mock_ul, &mock_translator}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    // Create a test packet
    typename decltype(mock_ul)::request_type seed;
    seed.address = champsim::address{0xdeadbeef};
    seed.v_address = champsim::address{0xdeadbeef};
    seed.is_translated = false;
    seed.cpu = 0;

    mock_ul.issue(seed);

    //for (auto elem : elements)
      //elem->_operate();

    WHEN("A packet is sent that matches the seed's virtual page but doesn't need to be translated") {
      typename decltype(mock_ul)::request_type test;
      test.address = champsim::address{0xcafebabe};
      test.v_address = champsim::address{0xdeadb000};
      test.is_translated = true;
      test.cpu = 0;

      mock_ul.issue(test);

      for (int i = 0; i < 20; ++i) {
        for (auto elem : elements)
          elem->_operate();
      }

      THEN("The address of the test packet is unmodified") {
        REQUIRE_THAT(mock_ll.addresses, Catch::Matchers::RangeEquals(std::vector{champsim::address{0x11111eef}, test.address}));
      }
    }
  }
}
