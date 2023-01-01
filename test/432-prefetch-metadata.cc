#include "catch.hpp"
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

#include <map>
#include <vector>

namespace test
{
  extern std::map<CACHE*, std::vector<uint32_t>> metadata_operate_collector;
  extern std::map<CACHE*, std::vector<uint32_t>> metadata_fill_collector;
  extern std::map<CACHE*, uint32_t> metadata_fill_emitter;
}

SCENARIO("Prefetch metadata from an issued prefetch is seen in the lower level") {
  GIVEN("An upper and lower level pair of caches") {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll;
    champsim::NonTranslatingQueues lower_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    champsim::NonTranslatingQueues upper_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE lower{"432-lower", 1, 1, 8, 32, fill_latency, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), lower_queues, &mock_ll, CACHE::ptestDmodulesDprefetcherDmetadata_collector, CACHE::rreplacementDlru};
    CACHE upper{"432-upper", 1, 1, 8, 32, fill_latency, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), upper_queues, &lower, CACHE::pprefetcherDno, CACHE::rreplacementDlru};

    std::array<champsim::operable*, 5> elements{{&mock_ll, &lower_queues, &upper_queues, &lower, &upper}};

    // Initialize the prefetching and replacement
    upper.initialize();
    lower.initialize();

    // Turn off warmup
    for (auto elem : elements) {
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("The upper level issues a prefetch with metadata") {
      // Request a prefetch
      constexpr uint64_t seed_addr = 0xdeadbeef;
      constexpr uint32_t seed_metadata = 0xcafebabe;
      auto seed_result = upper.prefetch_line(seed_addr, true, seed_metadata);
      REQUIRE(seed_result);

      // Run the uut for a bunch of cycles to clear it out of the PQ and fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The lower level sees the metadata in prefetcher_cache_operate()") {
        REQUIRE(std::size(test::metadata_operate_collector[&lower]) == 1);
        REQUIRE(test::metadata_operate_collector[&lower].front() == seed_metadata);
      }
    }
  }
}

SCENARIO("Prefetch metadata from an filled block is seen in the upper level") {
  GIVEN("An upper and lower level pair of caches") {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll;
    champsim::NonTranslatingQueues lower_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    champsim::NonTranslatingQueues upper_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE lower{"432-lower", 1, 1, 8, 32, fill_latency, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), lower_queues, &mock_ll, CACHE::ptestDmodulesDprefetcherDmetadata_emitter, CACHE::rreplacementDlru};
    CACHE upper{"432-upper", 1, 1, 8, 32, fill_latency, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), upper_queues, &lower, CACHE::ptestDmodulesDprefetcherDmetadata_collector, CACHE::rreplacementDlru};
    to_rq_MRP mock_ul{&upper};

    std::array<champsim::operable*, 6> elements{{&mock_ll, &lower_queues, &upper_queues, &lower, &upper, &mock_ul}};

    // Initialize the prefetching and replacement
    upper.initialize();
    lower.initialize();

    // Turn off warmup
    for (auto elem : elements) {
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("The upper level experiences a miss and the lower level emits metadata on the fill") {
      constexpr uint64_t seed_addr = 0xdeadbeef;
      constexpr uint32_t seed_metadata = 0xcafebabe;

      test::metadata_fill_emitter[&lower] = seed_metadata;

      PACKET seed;
      seed.address = seed_addr;
      seed.cpu = 0;
      auto seed_result = mock_ul.issue(seed);
      REQUIRE(seed_result);

      // Run the uut for a bunch of cycles to clear it out of the PQ and fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The upper level sees the metadata in prefetcher_cache_operate()") {
        //REQUIRE(std::size(test::metadata_collector[&upper]) == 1);
        REQUIRE(std::count(std::begin(test::metadata_fill_collector[&upper]), std::end(test::metadata_fill_collector[&upper]), seed_metadata) == 1);
      }
    }
  }
}

