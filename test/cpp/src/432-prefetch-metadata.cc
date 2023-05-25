#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
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
    champsim::channel lower_queues{};
    CACHE lower{CACHE::Builder{champsim::defaults::default_l1d}
      .name("432a-lower")
      .upper_levels({&lower_queues})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .prefetcher<CACHE::ptestDcppDmodulesDprefetcherDmetadata_collector>()
    };
    CACHE upper{CACHE::Builder{champsim::defaults::default_l1d}
      .name("432a-upper")
      .lower_level(&lower_queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
    };

    std::array<champsim::operable*, 3> elements{{&mock_ll, &lower, &upper}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("The upper level issues a prefetch with metadata") {
      test::metadata_operate_collector.insert_or_assign(&upper, std::vector<uint32_t>{});

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
        REQUIRE_THAT(test::metadata_operate_collector.at(&lower), Catch::Matchers::Contains(seed_metadata));
      }
    }
  }
}

SCENARIO("Prefetch metadata from an filled block is seen in the upper level") {
  GIVEN("An upper and lower level pair of caches") {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll;
    champsim::channel lower_queues{};
    to_rq_MRP mock_ul;

    CACHE lower{CACHE::Builder{champsim::defaults::default_l1d}
      .name("432b-lower")
      .upper_levels({&lower_queues})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .prefetcher<CACHE::ptestDcppDmodulesDprefetcherDmetadata_emitter>()
    };

    CACHE upper{CACHE::Builder{champsim::defaults::default_l1d}
      .name("432b-upper")
      .upper_levels({&mock_ul.queues})
      .lower_level(&lower_queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .prefetcher<CACHE::ptestDcppDmodulesDprefetcherDmetadata_collector>()
    };

    std::array<champsim::operable*, 4> elements{{&mock_ll, &lower, &upper, &mock_ul}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("The upper level experiences a miss and the lower level emits metadata on the fill") {
      constexpr uint64_t seed_addr = 0xdeadbeef;
      constexpr uint32_t seed_metadata = 0xcafebabe;

      test::metadata_fill_emitter.insert_or_assign(&lower, seed_metadata);
      test::metadata_fill_collector.insert_or_assign(&upper, std::vector<uint32_t>{});

      decltype(mock_ul)::request_type seed;
      seed.address = seed_addr;
      seed.cpu = 0;
      auto seed_result = mock_ul.issue(seed);
      REQUIRE(seed_result);

      // Run the uut for a bunch of cycles to clear it out of the PQ and fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The upper level sees the metadata in prefetcher_cache_operate()") {
        REQUIRE_THAT(test::metadata_fill_collector.at(&upper), Catch::Matchers::RangeEquals(std::vector{seed_metadata}));
      }
    }
  }
}

