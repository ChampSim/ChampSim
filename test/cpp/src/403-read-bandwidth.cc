#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"

TEMPLATE_TEST_CASE("The read queue respects the tag bandwidth", "", to_rq_MRP, to_wq_MRP, to_pq_MRP) {
  constexpr auto hit_latency = 4;
  constexpr auto fill_latency = 1;
  constexpr auto tag_bandwidth = 2;

  auto size = GENERATE(range<long>(1, 4*tag_bandwidth));

  GIVEN("A cache with a few elements") {
    do_nothing_MRC mock_ll;
    TestType mock_ul;
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("403-uut")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .tag_bandwidth(champsim::bandwidth::maximum_type{tag_bandwidth})
      .fill_bandwidth(champsim::bandwidth::maximum_type{10})
    };

    std::array<champsim::operable*, 3> elements{{&uut, &mock_ll, &mock_ul}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    // Get a list of packets
    champsim::block_number seed_base_addr{0xdeadbeef};
    std::vector<typename TestType::request_type> seeds;

    for (auto i = 0; i < size; ++i) {
      typename TestType::request_type seed;
      seed.address = champsim::address{seed_base_addr + i};
      seed.instr_id = (uint64_t)i;
      seed.cpu = 0;

      seeds.push_back(seed);
    }
    REQUIRE(seeds.back().address == champsim::address{seed_base_addr + (size-1)});

    for (auto &seed : seeds) {
      auto seed_result = mock_ul.issue(seed);
      REQUIRE(seed_result);
    }

    // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
    for (auto i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("The same packets are sent") {
      for (auto &pkt : seeds) {
        pkt.instr_id += 100;
      }

      for (auto &pkt : seeds) {
        auto test_result = mock_ul.issue(pkt);
        REQUIRE(test_result);
      }

      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      auto cycle = (size-1)/tag_bandwidth;

      THEN("Packet " + std::to_string(size-1) + " was served in cycle " + std::to_string(cycle)) {
        REQUIRE_THAT(mock_ul.packets.back(), champsim::test::ReturnedMatcher(hit_latency + cycle, 1));
      }
    }
  }
}
