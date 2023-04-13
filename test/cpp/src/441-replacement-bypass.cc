#include <catch.hpp>
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

#include <map>

namespace test
{
  extern std::map<CACHE*, uint32_t> evict_way;
}

SCENARIO("The replacement policy can bypass") {
  using namespace std::literals;
  GIVEN("A single cache") {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll;
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"441-uut", 1.0, 1, 1, 32, fill_latency, 1, 1, 0, false, false, false, 0, uut_queues, &mock_ll, CACHE::pprefetcherDno, CACHE::rtestDcppDmodulesDreplacementDmock_replacement};
    to_wq_MRP mock_ul_seed{&uut};
    to_rq_MRP mock_ul_test{&uut};

    std::array<champsim::operable*, 5> elements{{&mock_ll, &uut_queues, &uut, &mock_ul_seed, &mock_ul_test}};

    // Initialize the prefetching and replacement
    uut.initialize();

    // Turn off warmup
    for (auto elem : elements) {
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A packet is issued") {
      PACKET test;
      test.address = 0xdeadbeef;
      test.cpu = 0;
      test.type = WRITE;
      auto test_result = mock_ul_seed.issue(test);

      THEN("The issue is received") {
        REQUIRE(test_result);
      }

      // Run the uut for a bunch of cycles to fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      AND_WHEN("A packet with a different address is sent") {
        test::evict_way.insert_or_assign(&uut, 1);

        PACKET test_b;
        test_b.address = 0xcafebabe;
        test_b.cpu = 0;
        test_b.type = LOAD;
        test_b.instr_id = 1;

        auto test_b_result = mock_ul_test.issue(test_b);

        for (uint64_t i = 0; i < hit_latency+1; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The issue is received") {
          CHECK(test_b_result);
          CHECK(mock_ll.packet_count() == 1);
          CHECK(mock_ll.addresses.back() == test_b.address);
        }

        for (uint64_t i = 0; i < 2*(fill_latency+hit_latency); ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("No blocks are evicted") {
          REQUIRE(mock_ll.packet_count() == 1);
        }
      }
    }
  }
}
