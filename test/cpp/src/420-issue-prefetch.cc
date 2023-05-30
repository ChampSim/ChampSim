#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"
#include "champsim_constants.h"
#include "pref_interface.h"

namespace test
{
  extern std::map<CACHE*, std::vector<pref_cache_operate_interface>> prefetch_hit_collector;
}

SCENARIO("A prefetch can be issued") {
  GIVEN("An empty cache") {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{CACHE::Builder{champsim::defaults::default_l1d}
      .name("420-uut")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .prefetcher<CACHE::ptestDcppDmodulesDprefetcherDprefetch_hit_collector>()
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
      constexpr uint64_t seed_addr = 0xdeadbeef;
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
        test::prefetch_hit_collector.insert_or_assign(&uut, std::vector<test::pref_cache_operate_interface>{});

        // Create a test packet
        decltype(mock_ul)::request_type test;
        test.address = 0xdeadbeef;
        test.cpu = 0;

        auto test_result = mock_ul.issue(test);
        THEN("The issue is accepted") {
          REQUIRE(test_result);
        }

        for (uint64_t i = 0; i < 2*hit_latency; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The packet hits the cache") {
          REQUIRE(mock_ul.packets.back().return_time == mock_ul.packets.back().issue_time + hit_latency + 1);
        }

        THEN("The number of useful prefetches is incremented") {
          REQUIRE(uut.sim_stats.pf_issued == 1);
          REQUIRE(uut.sim_stats.pf_useful == 1);
        }

        THEN("The packet is shown to be a prefetch hit") {
          REQUIRE_THAT(test::prefetch_hit_collector.at(&uut), Catch::Matchers::SizeIs(1) && Catch::Matchers::AllMatch(
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

