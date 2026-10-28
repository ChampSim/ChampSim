#include <catch.hpp>

#include "cache.h"
#include "defaults.hpp"
#include "mocks.hpp"

SCENARIO("The prefetch queue size limits the number of prefetches that can be issued")
{
  GIVEN("An empty cache with a short prefetch queue")
  {
    auto pq_size = GENERATE(as<unsigned>(), 1, 3, 5, 16);
    do_nothing_MRC mock_ll;
    CACHE uut{champsim::modules::ModuleBuilder{"t426_cache", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
                  .add_parameter("mshr_size", static_cast<uint32_t>(8))
                  .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                  .add_parameter("pq_size", static_cast<std::size_t>(pq_size))};

    std::array<champsim::operable*, 2> elements{{&mock_ll, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    THEN("The internal prefetch queue size follows from the constructor")
    {
      REQUIRE_FALSE(uut.get_pq_size().empty());
      REQUIRE(uut.get_pq_size().back() == pq_size);
    }

    THEN("The initial internal prefetch queue occupancy is zero")
    {
      REQUIRE_FALSE(uut.get_pq_occupancy().empty());
      CHECK(uut.get_pq_occupancy().back() == 0);
      CHECK(uut.get_pq_occupancy_ratio().back() == 0);
    }

    WHEN(std::to_string(pq_size) + " prefetches are issued")
    {
      std::vector<bool> issue_results;
      std::generate_n(std::back_inserter(issue_results), pq_size, [&] {
        constexpr champsim::address seed_addr{0xdeadbeef};
        return uut.prefetch_line(seed_addr, true, 0);
      });

      THEN("The issue is accepted") { REQUIRE_THAT(issue_results, Catch::Matchers::AllTrue()); }

      THEN("The number of issued and requested prefetches increase")
      {
        CHECK(uut.sim_stats.pf_issued == pq_size);
        CHECK(uut.sim_stats.pf_requested == pq_size);
      }

      THEN("The initial internal prefetch queue occupancy increases")
      {
        REQUIRE_FALSE(uut.get_pq_occupancy().empty());
        CHECK(uut.get_pq_occupancy().back() == pq_size);
        CHECK(uut.get_pq_occupancy_ratio().back() == 1);
      }

      AND_WHEN("One more prefetch is issued")
      {
        auto test_result = uut.prefetch_line(champsim::address{0xcafebabe}, true, 0);

        THEN("The prefetch is rejected") { REQUIRE_FALSE(test_result); }

        THEN("The number of requested prefetches increase, but the number of issued prefetches does not")
        {
          CHECK(uut.sim_stats.pf_issued == pq_size);
          CHECK(uut.sim_stats.pf_requested == pq_size + 1);
        }
      }
    }
  }
}
