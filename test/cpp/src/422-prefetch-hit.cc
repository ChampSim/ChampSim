#include <catch.hpp>

#include "cache.h"
#include "defaults.hpp"
#include "mocks.hpp"

SCENARIO("A prefetch can hit the cache")
{
  GIVEN("A cache with one element")
  {
    constexpr uint64_t hit_latency = 1;
    constexpr uint64_t fill_latency = 10;
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::modules::ModuleBuilder{"t422_cache_0", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
                  .add_parameter("mshr_size", static_cast<uint32_t>(8))
                  .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                  .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                  .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
                  .add_parameter("fill_latency", static_cast<uint64_t>(fill_latency))};

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    decltype(mock_ul)::request_type seed;
    seed.address = champsim::address{0xdeadbeef};
    seed.instr_id = 1;
    seed.origin = champsim::origin{0, 0};

    auto seed_result = mock_ul.issue(seed);
    REQUIRE(seed_result);

    for (auto i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("A prefetch is issued with 'fill_this_level == true'")
    {
      auto test_result = uut.prefetch_line(seed.address, true, 0);
      REQUIRE(test_result);

      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The packet hits the cache") { REQUIRE(mock_ll.packet_count() == 1); }
    }
  }
}

SCENARIO("A prefetch not intended to fill this level that would hit the cache is ignored")
{
  GIVEN("A cache with one element")
  {
    constexpr uint64_t hit_latency = 1;
    constexpr uint64_t fill_latency = 10;
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::modules::ModuleBuilder{"t422_cache_1", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
                  .add_parameter("mshr_size", static_cast<uint32_t>(8))
                  .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                  .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                  .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
                  .add_parameter("fill_latency", static_cast<uint64_t>(fill_latency))};

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    decltype(mock_ul)::request_type seed;
    seed.address = champsim::address{0xdeadbeef};
    seed.instr_id = 1;
    seed.origin = champsim::origin{0, 0};

    auto seed_result = mock_ul.issue(seed);
    REQUIRE(seed_result);

    for (auto i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("A prefetch is issued with 'fill_this_level == false'")
    {
      auto test_result = uut.prefetch_line(seed.address, false, 0);
      REQUIRE(test_result);

      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The packet hits the cache") { REQUIRE(mock_ll.packet_count() == 1); }
    }
  }
}
