#include <catch.hpp>
#include "mocks.hpp"
#include "cache.h"
#include "defaults.hpp"
#include "modules.h"

template <uint64_t bypass_addr>
struct bypass_replacement : champsim::modules::replacement
{
  using replacement::replacement;
  long find_victim(uint32_t, uint64_t, long, const CACHE::BLOCK*, champsim::address, champsim::address addr, uint32_t)
  {
    if (addr == champsim::address{bypass_addr})
      return 1L;
    return 0L;
  }
};

SCENARIO("The replacement policy can bypass") {
  using namespace std::literals;
  GIVEN("A single cache") {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll;
    to_wq_MRP mock_ul_seed;
    to_rq_MRP mock_ul_test;
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l2c}
      .name("441-uut")
      .sets(1)
      .ways(1)
      .upper_levels({{&mock_ul_seed.queues, &mock_ul_test.queues}})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .offset_bits(champsim::data::bits{})
      .replacement<bypass_replacement<0xcafebabe>>()
    };

    std::array<champsim::operable*, 4> elements{{&mock_ll, &uut, &mock_ul_seed, &mock_ul_test}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A packet is issued") {
      decltype(mock_ul_seed)::request_type test;
      test.address = champsim::address{0xdeadbeef};
      test.cpu = 0;
      test.type = access_type::WRITE;
      auto test_result = mock_ul_seed.issue(test);

      THEN("The issue is received") {
        REQUIRE(test_result);
      }

      // Run the uut for a bunch of cycles to fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      AND_WHEN("A packet with a different address is sent") {
        decltype(mock_ul_test)::request_type test_b;
        test_b.address = champsim::address{0xcafebabe};
        test_b.cpu = 0;
        test_b.type = access_type::LOAD;
        test_b.instr_id = 1;

        auto test_b_result = mock_ul_test.issue(test_b);

        for (uint64_t i = 0; i < 2*hit_latency; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The issue is received") {
          CHECK(test_b_result);
          CHECK_THAT(mock_ll.addresses, Catch::Matchers::RangeEquals(std::vector{test_b.address}));
        }

        for (uint64_t i = 0; i < 2*(fill_latency+hit_latency); ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("No blocks are evicted") {
          REQUIRE_THAT(mock_ll.addresses, Catch::Matchers::RangeEquals(std::vector{test_b.address}));
        }
      }
    }
  }
}
