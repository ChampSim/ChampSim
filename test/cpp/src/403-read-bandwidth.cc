#include <catch.hpp>
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

SCENARIO("The read queue respects the tag bandwidth") {
  constexpr uint64_t hit_latency = 4;
  constexpr uint64_t fill_latency = 1;
  constexpr std::size_t tag_bandwidth = 2;

  auto size = GENERATE(range<std::size_t>(1, 4*tag_bandwidth));

  GIVEN("A cache with a few elements") {
    do_nothing_MRC mock_ll;
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"403-uut-"+std::to_string(size)+"r", 1, 1, 8, 32, fill_latency, tag_bandwidth, 10, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &mock_ll, CACHE::pprefetcherDno, CACHE::rreplacementDlru};
    to_rq_MRP warmup_ul{&uut}, mock_ul{&uut};

    std::array<champsim::operable*, 5> elements{{&mock_ll, &warmup_ul, &mock_ul, &uut_queues, &uut}};

    // Initialize the prefetching and replacement
    uut.initialize();

    // Turn off warmup
    uut.warmup = false;
    uut_queues.warmup = false;
    uut.begin_phase();
    uut_queues.begin_phase();

    // Get a list of packets
    uint64_t seed_base_addr = 0xdeadbeef;
    std::vector<PACKET> seeds;

    for (std::size_t i = 0; i < size; ++i) {
      PACKET seed;
      seed.address = seed_base_addr + i*BLOCK_SIZE;
      seed.instr_id = i;
      seed.cpu = 0;

      seeds.push_back(seed);
    }
    REQUIRE(seeds.back().address == seed_base_addr + (std::size(seeds)-1)*BLOCK_SIZE);

    for (auto &seed : seeds) {
      auto seed_result = warmup_ul.issue(seed);
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

SCENARIO("The prefetch queue respects the tag bandwidth") {
  constexpr uint64_t hit_latency = 4;
  constexpr uint64_t fill_latency = 1;
  constexpr std::size_t tag_bandwidth = 2;

  auto size = GENERATE(range<std::size_t>(1, 4*tag_bandwidth));

  GIVEN("A cache with a few elements") {
    do_nothing_MRC mock_ll;
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"403-uut-"+std::to_string(size)+"p", 1, 1, 8, 32, fill_latency, tag_bandwidth, 10, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &mock_ll, CACHE::pprefetcherDno, CACHE::rreplacementDlru};
    to_rq_MRP warmup_ul{&uut};
    to_pq_MRP mock_ul{&uut};

    std::array<champsim::operable*, 5> elements{{&mock_ll, &warmup_ul, &mock_ul, &uut_queues, &uut}};

    // Initialize the prefetching and replacement
    uut.initialize();

    // Turn off warmup
    uut.warmup = false;
    uut_queues.warmup = false;
    uut.begin_phase();
    uut_queues.begin_phase();

    // Get a list of packets
    uint64_t seed_base_addr = 0xcafebabe;
    std::vector<PACKET> seeds;

    for (std::size_t i = 0; i < size; ++i) {
      PACKET seed;
      seed.address = seed_base_addr + i*BLOCK_SIZE;
      seed.instr_id = i;
      seed.cpu = 0;

      seeds.push_back(seed);
    }
    REQUIRE(seeds.back().address == seed_base_addr + (std::size(seeds)-1)*BLOCK_SIZE);

    for (auto &seed : seeds) {
      auto seed_result = warmup_ul.issue(seed);
      REQUIRE(seed_result);
    }

    // Run the uut for a bunch of cycles to clear it out of the PQ and fill the cache
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

SCENARIO("The write queue respects the tag bandwidth") {
  constexpr uint64_t hit_latency = 4;
  constexpr uint64_t fill_latency = 1;
  constexpr std::size_t tag_bandwidth = 2;

  auto size = GENERATE(range<std::size_t>(1, 4*tag_bandwidth));
  auto lowest = GENERATE(true, false);

  GIVEN("A cache with a few elements where the lowest level is " + std::to_string(lowest)) {
    do_nothing_MRC mock_ll;
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"403-uut-"+std::to_string(size)+"w-"+std::to_string(lowest), 1, 1, 8, 32, fill_latency, tag_bandwidth, 10, 0, false, lowest, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &mock_ll, CACHE::pprefetcherDno, CACHE::rreplacementDlru};
    to_rq_MRP warmup_ul{&uut};
    to_wq_MRP mock_ul{&uut};

    std::array<champsim::operable*, 5> elements{{&mock_ll, &warmup_ul, &mock_ul, &uut_queues, &uut}};

    // Initialize the prefetching and replacement
    uut.initialize();

    // Turn off warmup
    uut.warmup = false;
    uut_queues.warmup = false;
    uut.begin_phase();
    uut_queues.begin_phase();

    // Get a list of packets
    uint64_t seed_base_addr = 0xdeadbeef;
    std::vector<PACKET> seeds;

    for (std::size_t i = 0; i < size; ++i) {
      PACKET seed;
      seed.address = seed_base_addr + i*BLOCK_SIZE;
      seed.instr_id = i;
      seed.cpu = 0;

      seeds.push_back(seed);
    }
    REQUIRE(seeds.back().address == seed_base_addr + (std::size(seeds)-1)*BLOCK_SIZE);

    for (auto &seed : seeds) {
      auto seed_result = warmup_ul.issue(seed);
      REQUIRE(seed_result);
    }

    // Run the uut for a bunch of cycles to clear it out of the WQ and fill the cache
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

