#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"

SCENARIO("The MSHR respects the fill bandwidth") {
  constexpr uint64_t hit_latency = 4;
  constexpr uint64_t fill_latency = 1;
  constexpr std::size_t fill_bandwidth = 2;

  auto size = GENERATE(range<std::size_t>(1, 3*fill_bandwidth));

  GIVEN("An empty cache") {
    release_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{CACHE::Builder{champsim::defaults::default_l1d}
      .name("404-uut-m")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .tag_bandwidth(10)
      .fill_bandwidth(fill_bandwidth)
    };

    std::array<champsim::operable*, 3> elements{{&uut, &mock_ll, &mock_ul}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    // Get a list of packets
    uint64_t seed_base_addr = 0xdeadbeef;
    std::vector<decltype(mock_ul)::request_type> seeds;

    for (std::size_t i = 0; i < size; ++i) {
      decltype(mock_ul)::request_type seed;
      seed.address = seed_base_addr + i*BLOCK_SIZE;
      seed.instr_id = i;
      seed.cpu = 0;

      seeds.push_back(seed);
    }
    REQUIRE(seeds.back().address == seed_base_addr + (std::size(seeds)-1)*BLOCK_SIZE);

    WHEN(std::to_string(size) + " packets are sent") {
      for (auto &seed : seeds) {
        auto seed_result = mock_ul.issue(seed);
        REQUIRE(seed_result);
      }

      // Give the cache enough time to miss
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      for (const auto &pkt : seeds)
        mock_ll.release(pkt.address);

      // Give the cache enough time to fill
      for (uint64_t i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      auto cycle = (size-1)/fill_bandwidth;

      THEN("Packet " + std::to_string(size-1) + " was served in cycle " + std::to_string(cycle)) {
        REQUIRE(mock_ul.packets.back().return_time == 100 + fill_latency + cycle);
      }
    }
  }
}

SCENARIO("Writebacks respect the fill bandwidth") {
  constexpr uint64_t hit_latency = 4;
  constexpr uint64_t fill_latency = 1;
  constexpr std::size_t fill_bandwidth = 2;

  auto size = GENERATE(range<std::size_t>(1, 4*fill_bandwidth));

  GIVEN("An empty cache") {
    do_nothing_MRC mock_ll{20};
    to_wq_MRP mock_ul;
    CACHE uut{CACHE::Builder{champsim::defaults::default_l1d}
      .name("404-uut-w")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .tag_bandwidth(10)
      .reset_wq_checks_full_addr()
      .fill_bandwidth(fill_bandwidth)
    };

    std::array<champsim::operable*, 3> elements{{&uut, &mock_ll, &mock_ul}};

    // Initialize the prefetching and replacement
    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    // Get a list of packets
    uint64_t seed_base_addr = 0xdeadbeef;
    std::vector<decltype(mock_ul)::request_type> seeds;

    for (std::size_t i = 0; i < size; ++i) {
      decltype(mock_ul)::request_type seed;
      seed.address = seed_base_addr + i*BLOCK_SIZE;
      seed.instr_id = i;
      seed.type = access_type::WRITE;
      seed.cpu = 0;

      seeds.push_back(seed);
    }
    REQUIRE(seeds.back().address == seed_base_addr + (std::size(seeds)-1)*BLOCK_SIZE);

    WHEN(std::to_string(size) + " packets are sent") {
      for (auto &seed : seeds) {
        auto seed_result = mock_ul.issue(seed);
        REQUIRE(seed_result);
      }

      // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      auto cycle = (size-1)/fill_bandwidth;

      THEN("No packets were forwarded to the lower level") {
        REQUIRE(mock_ll.packet_count() == 0);
      }

      THEN("Packet " + std::to_string(size-1) + " was served in cycle " + std::to_string(cycle)) {
        REQUIRE(mock_ul.packets.back().return_time == mock_ul.packets.back().issue_time + hit_latency + fill_latency + cycle);
      }
    }
  }
}


