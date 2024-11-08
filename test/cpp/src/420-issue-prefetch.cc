#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"
#include "pref_interface.h"
#include <map>
#include <vector>

namespace
{
  std::map<CACHE*, std::vector<test::pref_cache_operate_interface>> prefetch_hit_collector;
}

struct hit_collector : champsim::modules::prefetcher
{
  using prefetcher::prefetcher;

  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type, uint32_t metadata_in)
  {
    ::prefetch_hit_collector[intern_].push_back({addr, ip, cache_hit, useful_prefetch, type, metadata_in});
    return metadata_in;
  }

  uint32_t prefetcher_cache_fill(uint64_t, long, long, uint8_t, uint64_t, uint32_t metadata_in)
  {
    return metadata_in;
  }
};

SCENARIO("A prefetch can be issued") {
  GIVEN("An empty cache") {
    constexpr auto hit_latency = 2;
    constexpr auto fill_latency = 2;
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("420-uut")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .prefetcher<hit_collector>()
    };

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    THEN("The number of prefetches is zero") {
      REQUIRE(uut.sim_stats.pf_issued == 0);
      REQUIRE(uut.sim_stats.pf_useful == 0);
      REQUIRE(uut.sim_stats.pf_fill == 0);
    }

    THEN("The initial internal prefetch queue occupancy is zero") {
      REQUIRE(uut.get_pq_occupancy().back() == 0);
    }

    WHEN("A prefetch is issued") {
      champsim::address seed_addr{0xdeadbeef};
      auto seed_result = uut.prefetch_line(seed_addr, true, 0);

      THEN("The issue is accepted") {
        REQUIRE(seed_result);
      }

      THEN("The initial internal prefetch queue occupancy increases") {
        REQUIRE(uut.get_pq_occupancy().back() == 1);
      }

      // Run the uut for a bunch of cycles to clear it out of the PQ and fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The number of prefetch fills is incremented") {
        REQUIRE(uut.sim_stats.pf_fill == 1);
      }

      THEN("The initial internal prefetch queue occupancy returns to zero") {
        REQUIRE(uut.get_pq_occupancy().back() == 0);
      }

      AND_WHEN("A packet with the same address is sent") {
        ::prefetch_hit_collector.insert_or_assign(&uut, std::vector<test::pref_cache_operate_interface>{});

        // Create a test packet
        decltype(mock_ul)::request_type test;
        test.address = champsim::address{0xdeadbeef};
        test.cpu = 0;

        auto test_result = mock_ul.issue(test);
        THEN("The issue is accepted") {
          REQUIRE(test_result);
        }

        for (uint64_t i = 0; i < 2*hit_latency; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The packet hits the cache") {
          REQUIRE_THAT(mock_ul.packets.back(), champsim::test::ReturnedMatcher(hit_latency + 1, 1));
        }

        THEN("The number of useful prefetches is incremented") {
          REQUIRE(uut.sim_stats.pf_issued == 1);
          REQUIRE(uut.sim_stats.pf_useful == 1);
        }

        THEN("The packet is shown to be a prefetch hit") {
          REQUIRE_THAT(::prefetch_hit_collector.at(&uut), Catch::Matchers::SizeIs(1) && Catch::Matchers::AllMatch(
                Catch::Matchers::Predicate<test::pref_cache_operate_interface>(
                  [](test::pref_cache_operate_interface x){ return x.useful_prefetch; },
                  "is prefetch hit"
                )
              ));
        }
      }
    }
  }
}

