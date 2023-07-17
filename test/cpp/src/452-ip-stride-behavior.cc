#include <catch.hpp>
#include <algorithm>
#include <numeric>
#include "address.h"
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"
#include "champsim_constants.h"

#include "../../../prefetcher/ip_stride/ip_stride.h"

struct StrideMatcher : Catch::Matchers::MatcherGenericBase {
  champsim::block_number::difference_type stride;

  explicit StrideMatcher(champsim::block_number::difference_type s) : stride(s) {}

    template<typename Range>
    bool match(const Range& range) const {
      std::vector<decltype(stride)> diffs;
      return std::adjacent_find(std::cbegin(range), std::cend(range), [stride=stride](const auto& x, const auto& y){ return champsim::offset(champsim::block_number{x}, champsim::block_number{y}) != stride; }) == std::cend(range);
    }

    std::string describe() const override {
        return "has stride " + std::to_string(stride);
    }
};

SCENARIO("The ip_stride prefetcher issues prefetches when the IP matches") {
  auto stride = GENERATE(as<int64_t>{}, -4, -3, -2, -1, 1, 2, 3, 4);
  GIVEN("A cache with one filled block") {
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("452-uut-["+std::to_string(stride)+"]")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .prefetcher<ip_stride>()
    };

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    // Create a test packet
    static uint64_t id = 1;
    decltype(mock_ul)::request_type seed;
    seed.address = champsim::address{0xffff'003f};
    seed.ip = champsim::address{0xcafecafe};
    seed.instr_id = id++;
    seed.cpu = 0;

    // Issue it to the uut
    auto seed_result = mock_ul.issue(seed);
    THEN("The issue is accepted") {
      REQUIRE(seed_result);
    }

    // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
    for (auto i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("Two more packets with the same IP but strided address is sent") {
      auto test_a = seed;
      test_a.address = champsim::address{champsim::block_number{seed.address} + stride};
      test_a.instr_id = id++;

      auto test_result_a = mock_ul.issue(test_a);
      THEN("The first issue is accepted") {
        REQUIRE(test_result_a);
      }

      auto test_b = test_a;
      test_b.address = champsim::address{champsim::block_number{test_a.address} + stride};
      test_b.instr_id = id++;

      auto test_result_b = mock_ul.issue(test_b);
      THEN("The second issue is accepted") {
        REQUIRE(test_result_b);
      }

      for (uint64_t i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("A total of 6 requests were generated with the same stride") {
        REQUIRE_THAT(mock_ll.addresses, Catch::Matchers::SizeIs(6) && StrideMatcher{stride});
      }
    }
  }
}


