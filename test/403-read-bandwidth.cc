#include "catch.hpp"
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

SCENARIO("The read queue respects the read bandwidth") {
  GIVEN("A cache with a few elements") {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 1;
    constexpr std::size_t read_bandwidth = 2;
    do_nothing_MRC mock_ll;
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"403-uut", 1, 1, 8, 32, fill_latency, read_bandwidth, 10, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &mock_ll, CACHE::pprefetcherDno, CACHE::rreplacementDlru};
    to_rq_MRP warmup_ul{&uut}, mock_ul{&uut};

    std::array<champsim::operable*, 5> elements{{&mock_ll, &warmup_ul, &mock_ul, &uut_queues, &uut}};

    // Initialize the prefetching and replacement
    uut.impl_prefetcher_initialize();
    uut.impl_replacement_initialize();

    // Turn off warmup
    uut.warmup = false;
    uut_queues.warmup = false;
    uut.begin_phase();
    uut_queues.begin_phase();

    // Get a list of packets
    static auto id = 1;
    uint64_t seed_addr = 0xdeadbeef;
    std::array<PACKET, 2*read_bandwidth> seeds;
    for (auto &pkt : seeds) {
      pkt.address = seed_addr;
      pkt.instr_id = id++;
      pkt.cpu = 0;
      seed_addr += BLOCK_SIZE;
    }
    REQUIRE(seeds.back().address == 0xdeadbeef + (std::size(seeds)-1)*BLOCK_SIZE);

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
        pkt.instr_id = id++;
      }

      for (auto &pkt : seeds) {
        auto test_result = mock_ul.issue(pkt);
        REQUIRE(test_result);
      }

      for (uint64_t i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("Only the bandwidth number of packets were served this cycle") {
        CHECK(std::size(mock_ul.packets) == std::size(seeds));
        CHECK(mock_ul.packets.at(0).return_time == mock_ul.packets.at(0).issue_time + hit_latency);
        CHECK(mock_ul.packets.at(1).return_time == mock_ul.packets.at(1).issue_time + hit_latency);
        CHECK(mock_ul.packets.at(2).return_time == mock_ul.packets.at(2).issue_time + hit_latency + 1); // beyond bandwidth
        CHECK(mock_ul.packets.at(3).return_time == mock_ul.packets.at(3).issue_time + hit_latency + 1); // beyond bandwidth
      }
    }
  }
}

SCENARIO("The prefetch queue respects the read bandwidth") {
  GIVEN("A cache with a few elements") {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 1;
    constexpr std::size_t read_bandwidth = 2;
    do_nothing_MRC mock_ll;
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"403-uut", 1, 1, 8, 32, fill_latency, read_bandwidth, 10, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &mock_ll, CACHE::pprefetcherDno, CACHE::rreplacementDlru};
    to_rq_MRP warmup_ul{&uut};
    to_pq_MRP mock_ul{&uut};

    std::array<champsim::operable*, 5> elements{{&mock_ll, &warmup_ul, &mock_ul, &uut_queues, &uut}};

    // Initialize the prefetching and replacement
    uut.impl_prefetcher_initialize();
    uut.impl_replacement_initialize();

    // Turn off warmup
    uut.warmup = false;
    uut_queues.warmup = false;
    uut.begin_phase();
    uut_queues.begin_phase();

    // Get a list of packets
    static auto id = 1;
    uint64_t seed_addr = 0xdeadbeef;
    std::array<PACKET, 2*read_bandwidth> seeds;
    for (auto &pkt : seeds) {
      pkt.address = seed_addr;
      pkt.instr_id = id++;
      pkt.cpu = 0;
      seed_addr += BLOCK_SIZE;
    }
    REQUIRE(seeds.back().address == 0xdeadbeef + (std::size(seeds)-1)*BLOCK_SIZE);

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
        pkt.instr_id = id++;
      }

      for (auto &pkt : seeds) {
        auto test_result = mock_ul.issue(pkt);
        REQUIRE(test_result);
      }

      for (uint64_t i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("Only the bandwidth number of packets were served this cycle") {
        CHECK(std::size(mock_ul.packets) == std::size(seeds));
        CHECK(mock_ul.packets.at(0).return_time == mock_ul.packets.at(0).issue_time + hit_latency);
        CHECK(mock_ul.packets.at(1).return_time == mock_ul.packets.at(1).issue_time + hit_latency);
        CHECK(mock_ul.packets.at(2).return_time == mock_ul.packets.at(2).issue_time + hit_latency + 1); // beyond bandwidth
        CHECK(mock_ul.packets.at(3).return_time == mock_ul.packets.at(3).issue_time + hit_latency + 1); // beyond bandwidth
      }
    }
  }
}

