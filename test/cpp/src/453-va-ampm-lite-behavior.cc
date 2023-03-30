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

SCENARIO("The va_ampm_lite prefetcher issues prefetches when addresses stride in the positive direction") {
  auto stride = GENERATE(as<int64_t>{}, 1, 2, 3, 4);
  GIVEN("A cache with one filled block") {
    do_nothing_MRC mock_ll;
    do_nothing_MRC mock_lt;
    to_rq_MRP mock_ul{[](auto x, auto y){ return x.v_address == y.v_address; }};
    CACHE uut{"453-uut-["+std::to_string(stride)+"]", 1, 1, 8, 32, 1, 3, 1, 1, 0, false, false, true, (1<<LOAD)|(1<<PREFETCH), {&mock_ul.queues}, &mock_lt.queues, &mock_ll.queues, CACHE::pprefetcherDva_ampm_lite, CACHE::rreplacementDlru};

    std::array<champsim::operable*, 4> elements{{&mock_ll, &mock_lt, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    // Create a test packet
    static uint64_t id = 1;
    decltype(mock_ul)::request_type seed;
    seed.address = 0xdeadbeef0020;
    seed.v_address = seed.address;
    seed.ip = 0xcafecafe;
    seed.instr_id = id++;
    seed.cpu = 0;
    seed.is_translated = false;

    // Issue it to the uut
    auto seed_result = mock_ul.issue(seed);
    THEN("The issue is accepted") {
      REQUIRE(seed_result);
    }

    // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
    for (auto i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("Three more packets with the same IP but strided address is sent") {
      auto test_a = seed;
      test_a.address = static_cast<uint64_t>(seed.address + stride*BLOCK_SIZE);
      test_a.v_address = test_a.address;
      test_a.instr_id = id++;

      auto test_result_a = mock_ul.issue(test_a);
      THEN("The first issue is accepted") {
        REQUIRE(test_result_a);
      }

      auto test_b = test_a;
      test_b.address = static_cast<uint64_t>(test_a.address + stride*BLOCK_SIZE);
      test_b.v_address = test_b.address;
      test_b.instr_id = id++;

      auto test_result_b = mock_ul.issue(test_b);
      THEN("The second issue is accepted") {
        REQUIRE(test_result_b);
      }

      for (uint64_t i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      auto test_c = test_b;
      test_c.address = static_cast<uint64_t>(test_b.address + stride*BLOCK_SIZE);
      test_c.v_address = test_c.address;
      test_c.instr_id = id++;

      auto test_result_c = mock_ul.issue(test_c);
      THEN("The second issue is accepted") {
        REQUIRE(test_result_c);
      }

      for (uint64_t i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("A total of 5 requests were generated with the same stride") {
        REQUIRE_THAT(mock_ll.addresses, Catch::Matchers::SizeIs(5) && StrideMatcher{stride});
      }
    }
  }
}

SCENARIO("The va_ampm_lite prefetcher issues prefetches when addresses stride in the negative direction") {
  auto stride = GENERATE(as<int64_t>{}, -1, -2, -3, -4);
  GIVEN("A cache with one filled block") {
    do_nothing_MRC mock_ll;
    do_nothing_MRC mock_lt;
    to_rq_MRP mock_ul{[](auto x, auto y){ return x.v_address == y.v_address; }};
    CACHE uut{"453-uut-["+std::to_string(stride)+"]", 1, 1, 8, 32, 1, 3, 1, 1, 0, false, false, true, (1<<LOAD)|(1<<PREFETCH), {&mock_ul.queues}, &mock_lt.queues, &mock_ll.queues, CACHE::pprefetcherDva_ampm_lite, CACHE::rreplacementDlru};

    std::array<champsim::operable*, 4> elements{{&mock_ll, &mock_lt, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    // Create a test packet
    static uint64_t id = 1;
    decltype(mock_ul)::request_type seed;
    seed.address = 0xdeadbeefffe0;
    seed.v_address = seed.address;
    seed.ip = 0xcafecafe;
    seed.instr_id = id++;
    seed.cpu = 0;
    seed.is_translated = false;

    // Issue it to the uut
    auto seed_result = mock_ul.issue(seed);
    THEN("The issue is accepted") {
      REQUIRE(seed_result);
    }

    // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
    for (auto i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("Three more packets with the same IP but strided address is sent") {
      auto test_a = seed;
      test_a.address = static_cast<uint64_t>(seed.address + stride*BLOCK_SIZE);
      test_a.v_address = test_a.address;
      test_a.instr_id = id++;

      auto test_result_a = mock_ul.issue(test_a);
      THEN("The first issue is accepted") {
        REQUIRE(test_result_a);
      }

      auto test_b = test_a;
      test_b.address = static_cast<uint64_t>(test_a.address + stride*BLOCK_SIZE);
      test_b.v_address = test_b.address;
      test_b.instr_id = id++;

      auto test_result_b = mock_ul.issue(test_b);
      THEN("The second issue is accepted") {
        REQUIRE(test_result_b);
      }

      for (uint64_t i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      auto test_c = test_b;
      test_c.address = static_cast<uint64_t>(test_b.address + stride*BLOCK_SIZE);
      test_c.v_address = test_c.address;
      test_c.instr_id = id++;

      auto test_result_c = mock_ul.issue(test_c);
      THEN("The second issue is accepted") {
        REQUIRE(test_result_c);
      }

      for (uint64_t i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("A total of 5 requests were generated with the same stride") {
        REQUIRE_THAT(mock_ll.addresses, Catch::Matchers::SizeIs(5) && StrideMatcher{stride});
      }
    }
  }
}


