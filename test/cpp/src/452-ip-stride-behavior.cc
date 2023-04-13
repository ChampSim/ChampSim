#include <catch.hpp>
#include <numeric>
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

struct StrideMatcher : Catch::Matchers::MatcherGenericBase {
  int64_t stride;

  explicit StrideMatcher(int64_t s) : stride(s) {}

    template<typename Range>
    bool match(Range const& range) const {
      std::vector<int64_t> diffs;
      std::adjacent_difference(std::cbegin(range), std::cend(range), std::back_inserter(diffs), [](const auto& x, const auto& y){ return (x >> LOG2_BLOCK_SIZE) - (y >> LOG2_BLOCK_SIZE); });
      return std::all_of(std::next(std::cbegin(diffs)), std::cend(diffs), [stride=stride](auto x){ return x == stride; });
    }

    std::string describe() const override {
        return "has stride " + std::to_string(stride);
    }
};

SCENARIO("The ip_stride prefetcher issues prefetches when the IP matches") {
  auto stride = GENERATE(as<int64_t>{}, -4, -3, -2, -1, 1, 2, 3, 4);
  GIVEN("A cache with one filled block") {
    do_nothing_MRC mock_ll;
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, 1, LOG2_BLOCK_SIZE, false};
    CACHE uut{"452-uut-["+std::to_string(stride)+"]", 1, 1, 8, 32, 3, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &mock_ll, CACHE::pprefetcherDip_stride, CACHE::rreplacementDlru};
    to_rq_MRP mock_ul{&uut};

    std::array<champsim::operable*, 4> elements{{&mock_ll, &mock_ul, &uut_queues, &uut}};

    // Initialize the prefetching and replacement
    uut.initialize();

    // Turn off warmup
    uut.warmup = false;
    uut_queues.warmup = false;

    // Initialize stats
    uut.begin_phase();
    uut_queues.begin_phase();

    // Create a test packet
    static uint64_t id = 1;
    PACKET seed;
    seed.address = 0xffff'003f;
    seed.ip = 0xcafecafe;
    seed.instr_id = id++;
    seed.cpu = 0;
    seed.to_return = {&mock_ul};

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
      test_a.address = static_cast<uint64_t>(seed.address + stride*BLOCK_SIZE);
      test_a.instr_id = id++;

      auto test_result_a = mock_ul.issue(test_a);
      THEN("The first issue is accepted") {
        REQUIRE(test_result_a);
      }

      auto test_b = test_a;
      test_b.address = static_cast<uint64_t>(test_a.address + stride*BLOCK_SIZE);
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


