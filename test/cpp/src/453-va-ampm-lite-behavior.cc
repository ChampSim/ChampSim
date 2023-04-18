#include <catch.hpp>
#include <numeric>
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

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

SCENARIO("The va_ampm_lite prefetcher issues prefetches when addresses stride in the positive direction") {
  auto stride = GENERATE(as<typename champsim::block_number::difference_type>{}, 1, 2, 3, -1, -2, -3);
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
    seed.address = champsim::address{0xdeadbeef0800};
    seed.v_address = seed.address;
    seed.ip = champsim::address{0xcafecafe};
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
      test_a.address = champsim::address{champsim::block_number{seed.address} + stride};
      test_a.v_address = test_a.address;
      test_a.instr_id = id++;

      auto test_result_a = mock_ul.issue(test_a);
      THEN("The first issue is accepted") {
        REQUIRE(test_result_a);
      }

      auto test_b = test_a;
      test_b.address = champsim::address{champsim::block_number{test_a.address} + stride};
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
      test_c.address = champsim::address{champsim::block_number{test_b.address} + stride};
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
        REQUIRE_THAT(mock_ll.addresses, Catch::Matchers::SizeIs(5) && StrideMatcher{static_cast<int64_t>(stride)});
      }
    }
  }
}

/*
SCENARIO("The va_ampm_lite prefetcher issues prefetches when addresses stride in the negative direction") {
  auto stride = GENERATE(as<uint64_t>{}, 1, 2, 3, 4);
  GIVEN("A cache with one filled block") {
    do_nothing_MRC mock_ll;
    do_nothing_MRC mock_lt;
    to_rq_MRP mock_ul{[](auto x, auto y){ return x.v_address == y.v_address; }};
    CACHE uut{"453-uut-[-"+std::to_string(stride)+"]", 1, 1, 8, 32, 1, 3, 1, 1, 0, false, false, true, (1<<LOAD)|(1<<PREFETCH), {&mock_ul.queues}, &mock_lt.queues, &mock_ll.queues, CACHE::pprefetcherDva_ampm_lite, CACHE::rreplacementDlru};

    std::array<champsim::operable*, 4> elements{{&mock_ll, &mock_lt, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    // Create a test packet
    static uint64_t id = 1;
    decltype(mock_ul)::request_type seed;
    seed.address = champsim::address{0xdeadbeefffe0};
    seed.v_address = seed.address;
    seed.ip = champsim::address{0xcafecafe};
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
      test_a.address = champsim::address{champsim::block_number{seed.address} - stride};
      test_a.v_address = test_a.address;
      test_a.instr_id = id++;

      auto test_result_a = mock_ul.issue(test_a);
      THEN("The first issue is accepted") {
        REQUIRE(test_result_a);
      }

      auto test_b = test_a;
      test_b.address = champsim::address{champsim::block_number{test_a.address} - stride};
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
      test_c.address = champsim::address{champsim::block_number{test_b.address} + stride};
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
        REQUIRE_THAT(mock_ll.addresses, Catch::Matchers::SizeIs(5) && StrideMatcher{static_cast<int64_t>(-stride)});
      }
    }
  }
}
*/


