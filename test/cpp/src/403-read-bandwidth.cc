#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"
#include "champsim_constants.h"

TEMPLATE_TEST_CASE("The read queue respects the tag bandwidth", "", to_rq_MRP, to_wq_MRP, to_pq_MRP) {
  constexpr uint64_t hit_latency = 4;
  constexpr uint64_t fill_latency = 1;
  constexpr std::size_t tag_bandwidth = 2;

  auto size = GENERATE(range<std::size_t>(1, 4*tag_bandwidth));

  GIVEN("A cache with a few elements") {
    do_nothing_MRC mock_ll;
    TestType mock_ul;
    CACHE uut{CACHE::Builder{champsim::defaults::default_l1d}
      .name("403-uut")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .tag_bandwidth(tag_bandwidth)
      .fill_bandwidth(10)
    };

    std::array<champsim::operable*, 3> elements{{&uut, &mock_ll, &mock_ul}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    // Get a list of packets
    uint64_t seed_base_addr = 0xdeadbeef;
    std::vector<typename TestType::request_type> seeds;

    for (std::size_t i = 0; i < size; ++i) {
      typename TestType::request_type seed;
      seed.address = seed_base_addr + i*BLOCK_SIZE;
      seed.instr_id = i;
      seed.cpu = 0;

      seeds.push_back(seed);
    }
    REQUIRE(seeds.back().address == seed_base_addr + (std::size(seeds)-1)*BLOCK_SIZE);

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

      for (uint64_t i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      auto cycle = (size-1)/tag_bandwidth;

      THEN("Packet " + std::to_string(size-1) + " was served in cycle " + std::to_string(cycle)) {
        REQUIRE(mock_ul.packets.back().return_time == mock_ul.packets.back().issue_time + hit_latency + cycle);
      }
    }
  }
}
