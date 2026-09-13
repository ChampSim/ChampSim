#include <catch.hpp>
#include <map>
#include <vector>

#include "cache.h"
#include "defaults.hpp"
#include "mocks.hpp"
#include "pref_interface.h"

namespace
{
std::map<champsim::modules::cache_module*, std::vector<test::pref_cache_operate_interface>> prefetch_hit_collector;
}

struct hit_collector : champsim::modules::prefetcher {
  using prefetcher::prefetcher;

  champsim::modules::cache_module* parent_ = nullptr;

  void prefetcher_initialize() override {}
  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, bool cache_hit, bool useful_prefetch, access_type type, uint32_t metadata_in) override
  {
    ::prefetch_hit_collector[parent_].push_back({addr, ip, cache_hit, useful_prefetch, type, metadata_in});
    return metadata_in;
  }

  uint32_t prefetcher_cache_fill(champsim::address, long, long, bool, champsim::address, uint32_t metadata_in) override
  {
    return metadata_in;
  }

  void prefetcher_cycle_operate() override {}
  void prefetcher_final_stats() override {}
  void prefetcher_branch_operate(champsim::address, uint8_t, champsim::address) override {}

  hit_collector(champsim::modules::ModuleBuilder builder)
    : parent_(builder.get_parent<champsim::modules::cache_module>()) {}
};

champsim::modules::prefetcher::register_module<hit_collector> hit_collector_register("hit_collector");

SCENARIO("A prefetch can be issued") {
  GIVEN("An empty cache") {
    constexpr auto hit_latency = 2;
    constexpr auto fill_latency = 2;
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::modules::ModuleBuilder{"t420_cache", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
      .add_parameter("mshr_size", static_cast<uint32_t>(8))
      .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
      .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
      .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
      .add_parameter("fill_latency", static_cast<uint64_t>(fill_latency))
      .add_submodule("prefetcher", champsim::modules::ModuleBuilder{"t420_hit_collector", "hit_collector"})
    };

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    THEN("The number of prefetches is zero")
    {
      REQUIRE(uut.sim_stats.pf_issued == 0);
      REQUIRE(uut.sim_stats.pf_useful == 0);
      REQUIRE(uut.sim_stats.pf_fill == 0);
    }

    THEN("The initial internal prefetch queue occupancy is zero") { REQUIRE(uut.get_pq_occupancy().back() == 0); }

    WHEN("A prefetch is issued")
    {
      champsim::address seed_addr{0xdeadbeef};
      auto seed_result = uut.prefetch_line(seed_addr, true, 0);

      THEN("The issue is accepted") { REQUIRE(seed_result); }

      THEN("The initial internal prefetch queue occupancy increases") { REQUIRE(uut.get_pq_occupancy().back() == 1); }

      // Run the uut for a bunch of cycles to clear it out of the PQ and fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The number of prefetch fills is incremented") { REQUIRE(uut.sim_stats.pf_fill == 1); }

      THEN("The initial internal prefetch queue occupancy returns to zero") { REQUIRE(uut.get_pq_occupancy().back() == 0); }

      AND_WHEN("A packet with the same address is sent")
      {
        ::prefetch_hit_collector.insert_or_assign(&uut, std::vector<test::pref_cache_operate_interface>{});

        // Create a test packet
        decltype(mock_ul)::request_type test;
        test.address = champsim::address{0xdeadbeef};
        test.origin = champsim::origin{0, 0};

        auto test_result = mock_ul.issue(test);
        THEN("The issue is accepted") { REQUIRE(test_result); }

        for (uint64_t i = 0; i < 2 * hit_latency; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The packet hits the cache") { REQUIRE_THAT(mock_ul.packets.back(), champsim::test::ReturnedMatcher(hit_latency + 1, 1)); }

        THEN("The number of useful prefetches is incremented")
        {
          REQUIRE(uut.sim_stats.pf_issued == 1);
          REQUIRE(uut.sim_stats.pf_useful == 1);
        }

        THEN("The packet is shown to be a prefetch hit")
        {
          REQUIRE_THAT(::prefetch_hit_collector.at(&uut), Catch::Matchers::SizeIs(1)
                                                              && Catch::Matchers::AllMatch(Catch::Matchers::Predicate<test::pref_cache_operate_interface>(
                                                                  [](test::pref_cache_operate_interface x) { return x.useful_prefetch; }, "is prefetch hit")));
        }
      }
    }
  }
}
