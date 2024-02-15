#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"

SCENARIO("A prefetch can hit the cache") {
  GIVEN("A cache with one element") {
    constexpr uint64_t hit_latency = 1;
    constexpr uint64_t fill_latency = 10;
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("422-uut")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
    };

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    decltype(mock_ul)::request_type seed;
    seed.address = champsim::address{0xdeadbeef};
    seed.instr_id = 1;
    seed.cpu = 0;

    auto seed_result = mock_ul.issue(seed);
    REQUIRE(seed_result);

    for (auto i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("A prefetch is issued with 'fill_this_level == true'") {
      auto test_result = uut.prefetch_line(seed.address, true, 0);
      REQUIRE(test_result);

      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The packet hits the cache") {
        REQUIRE(mock_ll.packet_count() == 1);
      }
    }
  }
}

SCENARIO("A prefetch not intended to fill this level that would hit the cache is ignored") {
  GIVEN("A cache with one element") {
    constexpr uint64_t hit_latency = 1;
    constexpr uint64_t fill_latency = 10;
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("422-uut")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
    };

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    decltype(mock_ul)::request_type seed;
    seed.address = champsim::address{0xdeadbeef};
    seed.instr_id = 1;
    seed.cpu = 0;

    auto seed_result = mock_ul.issue(seed);
    REQUIRE(seed_result);

    for (auto i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("A prefetch is issued with 'fill_this_level == false'") {
      auto test_result = uut.prefetch_line(seed.address, false, 0);
      REQUIRE(test_result);

      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The packet hits the cache") {
        REQUIRE(mock_ll.packet_count() == 1);
      }
    }
  }
}

