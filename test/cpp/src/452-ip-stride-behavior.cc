#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"
#include "champsim_constants.h"

SCENARIO("The ip_stride prefetcher issues prefetches when the IP matches") {
  auto stride = GENERATE(as<int64_t>{}, -4, -3, -2, -1, 1, 2, 3, 4);
  GIVEN("A cache with one filled block") {
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{CACHE::Builder{champsim::defaults::default_l1d}
      .name("452-uut-["+std::to_string(stride)+"]")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .prefetcher<champsim::modules::generated::prefetcherDip_stride>()
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
    seed.address = 0xffff'003f;
    seed.ip = 0xcafecafe;
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

      THEN("A total of 6 requests were generated") {
        REQUIRE(mock_ll.packet_count() == 6);
      }

      THEN("All of the issued requests have the same stride") {
        REQUIRE((mock_ll.addresses.at(0) >> LOG2_BLOCK_SIZE) + stride == (mock_ll.addresses.at(1) >> LOG2_BLOCK_SIZE));
        REQUIRE((mock_ll.addresses.at(1) >> LOG2_BLOCK_SIZE) + stride == (mock_ll.addresses.at(2) >> LOG2_BLOCK_SIZE));
        REQUIRE((mock_ll.addresses.at(2) >> LOG2_BLOCK_SIZE) + stride == (mock_ll.addresses.at(3) >> LOG2_BLOCK_SIZE));
        REQUIRE((mock_ll.addresses.at(3) >> LOG2_BLOCK_SIZE) + stride == (mock_ll.addresses.at(4) >> LOG2_BLOCK_SIZE));
        REQUIRE((mock_ll.addresses.at(4) >> LOG2_BLOCK_SIZE) + stride == (mock_ll.addresses.at(5) >> LOG2_BLOCK_SIZE));
      }
    }
  }
}


