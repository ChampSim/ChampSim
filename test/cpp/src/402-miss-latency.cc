#include <catch.hpp>

#include "cache.h"
#include "defaults.hpp"
#include "mocks.hpp"

SCENARIO("A cache returns a miss after the specified latency")
{
  using namespace std::literals;
  auto [type, str] =
      GENERATE(table<access_type, std::string_view>({std::pair{access_type::LOAD, "load"sv}, std::pair{access_type::RFO, "RFO"sv},
                                                     std::pair{access_type::PREFETCH, "prefetch"sv}, std::pair{access_type::TRANSLATION, "translation"sv}}));

  GIVEN("An empty cache")
  {
    constexpr auto hit_latency = 4;
    constexpr auto miss_latency = 3;
    constexpr auto fill_latency = 2;
    do_nothing_MRC mock_ll{miss_latency};
    to_rq_MRP mock_ul;
    CACHE uut{champsim::modules::ModuleBuilder{"t402_cache_0", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
                  .add_parameter("mshr_size", static_cast<uint32_t>(8))
                  .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                  .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                  .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
                  .add_parameter("fill_latency", static_cast<uint64_t>(fill_latency))
                  .add_parameter("pref_activate_mask", std::vector<access_type>{access_type::LOAD, access_type::RFO, access_type::PREFETCH, access_type::WRITE, access_type::TRANSLATION})};

    std::array<champsim::operable*, 3> elements{{&uut, &mock_ll, &mock_ul}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    THEN("The MSHR occupancy starts at zero")
    {
      CHECK(uut.get_mshr_occupancy() == 0);
      CHECK(uut.get_mshr_occupancy_ratio() == 0);
    }

    WHEN("A " + std::string{str} + " packet is issued")
    {
      // Create a test packet
      static uint64_t id = 1;
      decltype(mock_ul)::request_type test;
      test.address = champsim::address{0xdeadbeef};
      test.origin = champsim::origin{0, 0};
      test.instr_id = id++;
      test.type = type;

      const auto initial_misses = uut.sim_stats.misses.value_or(std::pair{test.type, test.origin.cpu()}, 0);

      // Issue it to the uut
      auto test_result = mock_ul.issue(test);
      THEN("This issue is received") { REQUIRE(test_result); }

      // Run the uut for long enough to miss
      for (uint64_t i = 0; i < hit_latency + 1; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The MSHR occupancy increases")
      {
        CHECK(uut.get_mshr_occupancy() == 1);
        CHECK(uut.get_mshr_occupancy_ratio() > 0);
        CHECK(uut.get_mshr_occupancy_ratio() == (std::ceil(uut.get_mshr_occupancy()) / std::ceil(uut.get_mshr_size())));
      }

      // Run the uut for long enough to fill the cache
      for (uint64_t i = 0; i < 2 * (miss_latency + fill_latency); ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("It takes exactly the specified cycles to return")
      {
        REQUIRE_THAT(mock_ul.packets.front(),
                     champsim::test::ReturnedMatcher((fill_latency + miss_latency + hit_latency + 1), 1)); // +1 due to ordering of elements
      }

      THEN("The number of misses increases") { REQUIRE(uut.sim_stats.misses.value_or(std::pair{test.type, test.origin.cpu()}, 0) == initial_misses + 1); }

      THEN("The average miss latency increases only on demand fetches")
      {
        REQUIRE(uut.sim_stats.total_miss_latency_cycles == (type != access_type::PREFETCH ? miss_latency + fill_latency : 0));
      }

      THEN("The end-of-phase average miss latency increases only on demand fetches")
      {
        REQUIRE(uut.sim_stats.total_miss_latency_cycles == (type != access_type::PREFETCH ? miss_latency + fill_latency : 0));
      }
    }
  }
}

SCENARIO("A cache completes a fill after the specified latency")
{
  using namespace std::literals;
  auto [type, str] = std::pair{access_type::WRITE, "write"sv};
  auto match_offset = GENERATE(true, false);

  GIVEN("An empty cache")
  {
    constexpr auto hit_latency = 4;
    constexpr auto miss_latency = 3;
    constexpr auto fill_latency = 2;
    do_nothing_MRC mock_ll{miss_latency};
    to_wq_MRP mock_ul;
    auto builder = champsim::modules::ModuleBuilder{"t402_cache_1", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
                       .add_parameter("mshr_size", static_cast<uint32_t>(8))
                       .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                       .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                       .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
                       .add_parameter("fill_latency", static_cast<uint64_t>(fill_latency))
                       .add_parameter("pref_activate_mask", std::vector<access_type>{access_type::LOAD, access_type::RFO, access_type::PREFETCH, access_type::WRITE, access_type::TRANSLATION});

    if (match_offset)
      builder = builder.add_parameter("match_offset_bits", true);
    else
      builder = builder.add_parameter("match_offset_bits", false);

    CACHE uut{builder};

    std::array<champsim::operable*, 3> elements{{&uut, &mock_ll, &mock_ul}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    WHEN("A " + std::string{str} + " packet is issued")
    {
      // Create a test packet
      static uint64_t id = 1;
      decltype(mock_ul)::request_type test;
      test.address = champsim::address{0xdeadbeef};
      test.origin = champsim::origin{0, 0};
      test.instr_id = id++;
      test.type = type;

      const auto initial_misses = uut.sim_stats.misses.value_or(std::pair{test.type, test.origin.cpu()}, 0);

      // Issue it to the uut
      auto test_result = mock_ul.issue(test);
      THEN("This issue is received") { REQUIRE(test_result); }

      // Run the uut for a bunch of cycles to clear it out of the WQ and fill the cache
      for (uint64_t i = 0; i < 2 * (miss_latency + hit_latency + fill_latency); ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("It takes exactly the specified cycles to return")
      {
        if (match_offset)
          REQUIRE_THAT(mock_ul.packets.front(),
                       champsim::test::ReturnedMatcher((fill_latency + miss_latency + hit_latency + 1), 1)); // +1 due to ordering of elements
        else
          REQUIRE_THAT(mock_ul.packets.front(), champsim::test::ReturnedMatcher((fill_latency + hit_latency), 1)); // +1 due to ordering of elements
      }

      THEN("The number of misses increases") { REQUIRE(uut.sim_stats.misses.value_or(std::pair{test.type, test.origin.cpu()}, 0) == initial_misses + 1); }

      THEN("The average miss latency increases")
      {
        if (match_offset)
          REQUIRE(uut.sim_stats.total_miss_latency_cycles == miss_latency + fill_latency);
        else
          REQUIRE(uut.sim_stats.total_miss_latency_cycles == fill_latency - 1); // -1 due to ordering of elements
      }

      THEN("The end-of-phase average miss latency increases")
      {
        if (match_offset) {
          REQUIRE(uut.sim_stats.total_miss_latency_cycles == (miss_latency + fill_latency));
        } else {
          REQUIRE(uut.sim_stats.total_miss_latency_cycles == (fill_latency - 1)); // -1 due to ordering of elements
        }
      }
    }
  }
}

SCENARIO("The MSHR bandwidth limits the number of outstanding misses")
{
  GIVEN("An cache with full MSHRs")
  {
    release_MRC mock_ll;
    to_rq_MRP mock_ul_seed;
    to_rq_MRP mock_ul_test;
    CACHE uut{champsim::modules::ModuleBuilder{"t402_cache_2", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
                  .add_parameter("mshr_size", static_cast<uint32_t>(8))
                  .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul_seed.queues, &mock_ul_test.queues})
                  .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                  .add_parameter("mshr_size", static_cast<uint32_t>(1))};

    std::array<champsim::operable*, 4> elements{{&mock_ll, &uut, &mock_ul_seed, &mock_ul_test}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    uint64_t id = 1;
    decltype(mock_ul_seed)::request_type test_a;
    test_a.address = champsim::address{0xdeadbeef};
    test_a.origin = champsim::origin{0, 0};
    test_a.type = access_type::LOAD;
    test_a.instr_id = id++;

    auto test_a_result = mock_ul_seed.issue(test_a);

    for (uint64_t i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    THEN("The issue is received")
    {
      CHECK(test_a_result);
      CHECK(mock_ll.packet_count() == 1);
    }

    WHEN("A packet with a different address is sent before the fill has completed")
    {
      decltype(mock_ul_test)::request_type test_b;
      test_b.address = champsim::address{0xcafebabe};
      test_b.origin = champsim::origin{0, 0};
      test_b.type = access_type::LOAD;
      test_b.instr_id = id++;

      const auto initial_misses = uut.sim_stats.misses.value_or(std::pair{test_b.type, test_b.origin.cpu()}, 0);

      auto test_b_result = mock_ul_test.issue(test_b);

      THEN("The issue is received") { REQUIRE(test_b_result); }

      auto first_packet_delay = 10;
      for (auto i = 0; i < first_packet_delay; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The test packet was not forwarded to the lower level") { REQUIRE(mock_ll.packet_count() == 1); }

      THEN("The number of misses did not increase") { REQUIRE(uut.sim_stats.misses.value_or(std::pair{access_type::LOAD, 0}, 0) == initial_misses); }
    }
  }
}

SCENARIO("A lower-level queue refusal limits the number of outstanding misses")
{
  GIVEN("An empty cache")
  {
    champsim::channel refusal_channel{champsim::modules::ModuleBuilder{"t402_refusal_channel", "DEFAULT_CHANNEL", champsim::defaults::default_channel()}.add_parameter("rq_size", static_cast<std::size_t>(0)).add_parameter("wq_size", static_cast<std::size_t>(0)).add_parameter("pq_size", static_cast<std::size_t>(0))}; // Refuses all packets (zero-sized queues)
    to_rq_MRP mock_ul;
    CACHE uut{champsim::modules::ModuleBuilder{"t402_cache_3", "DEFAULT_CACHE", champsim::defaults::default_l1d().add_parameter("mshr_size", static_cast<uint32_t>(8))}.add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues}).add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&refusal_channel))};

    std::array<champsim::operable*, 2> elements{{&uut, &mock_ul}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    WHEN("A packet is sent")
    {
      uint64_t id = 1;
      decltype(mock_ul)::request_type test_a;
      test_a.address = champsim::address{0xdeadbeef};
      test_a.origin = champsim::origin{0, 0};
      test_a.type = access_type::LOAD;
      test_a.instr_id = id++;

      const auto initial_misses = uut.sim_stats.misses.value_or(std::pair{test_a.type, test_a.origin.cpu()}, 0);

      auto test_a_result = mock_ul.issue(test_a);

      for (uint64_t i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The issue is received") { CHECK(test_a_result); }

      auto first_packet_delay = 10;
      for (auto i = 0; i < first_packet_delay; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The number of misses did not increase") { REQUIRE(uut.sim_stats.misses.value_or(std::pair{test_a.type, test_a.origin.cpu()}, 0) == initial_misses); }
    }
  }
}
