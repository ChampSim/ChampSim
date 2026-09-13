#include <catch.hpp>
#include <map>
#include <vector>

#include "cache.h"
#include "defaults.hpp"
#include "mocks.hpp"
#include "modules.h"
#include "repl_interface.h"

namespace
{
std::map<champsim::modules::cache_module*, std::vector<test::repl_update_interface>> replacement_update_state_collector;
std::map<champsim::modules::cache_module*, std::vector<test::repl_fill_interface>> replacement_cache_fill_collector;
} // namespace

struct update_state_collector : champsim::modules::replacement {
  using replacement::replacement;

  champsim::modules::cache_module* parent_ = nullptr;

  update_state_collector(CACHE* c) {(void)c;}

  void initialize_replacement() override {}
  long find_victim(champsim::origin, uint64_t, long, const champsim::cache_block*, champsim::address, champsim::address, access_type) override
  {
    return 0;
  }

  void update_replacement_state(champsim::origin origin, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                access_type type, bool hit) override
  {
    auto usc_it = ::replacement_update_state_collector.try_emplace(parent_);
    usc_it.first->second.push_back({origin.cpu(), set, way, full_addr, ip, victim_addr, type, hit});
  }

  void replacement_cache_fill(champsim::origin origin, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                              access_type type) override
  {
    auto cfc_it = ::replacement_cache_fill_collector.try_emplace(parent_);
    cfc_it.first->second.push_back({origin.cpu(), set, way, full_addr, ip, victim_addr, type});
  }

  void replacement_final_stats() override {}

  update_state_collector(champsim::modules::ModuleBuilder builder)
    : parent_(builder.get_parent<champsim::modules::cache_module>()) {}
};

champsim::modules::replacement::register_module<update_state_collector> update_state_collect_register("update_state_collector");

SCENARIO("The replacement policy is triggered on a miss, not on a fill") {
  using namespace std::literals;
  auto [type, str] = GENERATE(table<access_type, std::string_view>({std::pair{access_type::LOAD, "load"sv}, std::pair{access_type::RFO, "RFO"sv},
                                                                    std::pair{access_type::PREFETCH, "prefetch"sv}, std::pair{access_type::WRITE, "write"sv},
                                                                    std::pair{access_type::TRANSLATION, "translation"sv}}));
  GIVEN("A single cache")
  {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    release_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::modules::ModuleBuilder{"t442_cache_0", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
      .add_parameter("mshr_size", static_cast<uint32_t>(8))
      .add_parameter("num_sets", static_cast<uint32_t>(1))
      .add_parameter("num_ways", static_cast<uint32_t>(1))
      .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
      .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
      .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
      .add_parameter("fill_latency", static_cast<uint64_t>(fill_latency))
      .add_parameter("pref_activate_mask", std::vector<access_type>{type})
      .add_parameter("offset_bits", champsim::data::bits{})
      .add_submodule("replacement", champsim::modules::ModuleBuilder{"t442_update_state_collector_0", "update_state_collector"})
      .add_submodule("replacement", champsim::modules::ModuleBuilder{"t442_lru_0", "lru"})
    };

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    WHEN("A " + std::string{str} + " is issued")
    {
      ::replacement_update_state_collector[&uut].clear();

      decltype(mock_ul)::request_type test;
      test.address = champsim::address{0xdeadbeef};
      test.is_translated = true;
      test.origin = champsim::origin{0, 0};
      test.type = type;
      auto test_result = mock_ul.issue(test);

      THEN("The issue is received") { REQUIRE(test_result); }

      // Run the uut for a bunch of cycles to fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The replacement policy is called with information from the issued packet")
      {
        REQUIRE_THAT(::replacement_update_state_collector[&uut], Catch::Matchers::SizeIs(1));
        CHECK(::replacement_update_state_collector[&uut].at(0).cpu == test.origin.cpu());
        CHECK(::replacement_update_state_collector[&uut].at(0).set == 0);
        CHECK(::replacement_update_state_collector[&uut].at(0).way == 1);
        CHECK(::replacement_update_state_collector[&uut].at(0).full_addr == test.address);
        CHECK(::replacement_update_state_collector[&uut].at(0).victim_addr == champsim::address{});
        CHECK(::replacement_update_state_collector[&uut].at(0).type == test.type);
        CHECK(::replacement_update_state_collector[&uut].at(0).hit == false);
      }

      AND_WHEN("The packet is returned")
      {
        mock_ll.release(test.address);

        // Run the uut for a bunch of cycles to fill the cache
        for (auto i = 0; i < 100; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The replacement policy is not called") { REQUIRE_THAT(::replacement_update_state_collector[&uut], Catch::Matchers::SizeIs(1)); }
      }
    }
  }
}

SCENARIO("The replacement policy is triggered on a hit")
{
  using namespace std::literals;
  auto [type, str] = GENERATE(table<access_type, std::string_view>({std::pair{access_type::LOAD, "load"sv}, std::pair{access_type::RFO, "RFO"sv},
                                                                    std::pair{access_type::PREFETCH, "prefetch"sv}, std::pair{access_type::WRITE, "write"sv},
                                                                    std::pair{access_type::TRANSLATION, "translation"sv}}));
  GIVEN("A cache with one element")
  {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::modules::ModuleBuilder{"t442_cache_1", "DEFAULT_CACHE", champsim::defaults::default_l2c()}
      .add_parameter("mshr_size", static_cast<uint32_t>(8))
      .add_parameter("num_sets", static_cast<uint32_t>(1))
      .add_parameter("num_ways", static_cast<uint32_t>(1))
      .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
      .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
      .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
      .add_parameter("fill_latency", static_cast<uint64_t>(fill_latency))
      .add_parameter("pref_activate_mask", std::vector<access_type>{type})
      .add_parameter("offset_bits", champsim::data::bits{})
      .add_submodule("replacement", champsim::modules::ModuleBuilder{"t442_update_state_collector_1", "update_state_collector"})
      .add_submodule("replacement", champsim::modules::ModuleBuilder{"t442_lru_1", "lru"})
    };

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    decltype(mock_ul)::request_type test;
    test.address = champsim::address{0xdeadbeef};
    test.address = champsim::address{0xdeadbeef};
    test.origin = champsim::origin{0, 0};
    test.type = type;
    auto test_result = mock_ul.issue(test);

    THEN("The issue is received") { REQUIRE(test_result); }

    // Run the uut for a bunch of cycles to fill the cache
    for (auto i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("A packet with the same address is issued")
    {
      ::replacement_update_state_collector[&uut].clear();
      auto repeat_test_result = mock_ul.issue(test);

      THEN("The issue is received") { REQUIRE(repeat_test_result); }

      // Run the uut for a bunch of cycles to fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The replacement policy is called with information from the issued packet")
      {
        REQUIRE_THAT(::replacement_update_state_collector[&uut], Catch::Matchers::SizeIs(1));
        CHECK(::replacement_update_state_collector[&uut].at(0).cpu == test.origin.cpu());
        CHECK(::replacement_update_state_collector[&uut].at(0).set == 0);
        CHECK(::replacement_update_state_collector[&uut].at(0).way == 0);
        CHECK(::replacement_update_state_collector[&uut].at(0).full_addr == test.address);
        CHECK(::replacement_update_state_collector[&uut].at(0).victim_addr == champsim::address{});
        CHECK(::replacement_update_state_collector[&uut].at(0).type == test.type);
        CHECK(::replacement_update_state_collector[&uut].at(0).hit == true);
      }
    }
  }
}

SCENARIO("The replacement policy notes the correct eviction information")
{
  GIVEN("An empty cache")
  {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll;
    to_wq_MRP mock_ul_seed;
    to_rq_MRP mock_ul_test;
    CACHE uut{champsim::modules::ModuleBuilder{"t442_cache_2", "DEFAULT_CACHE", champsim::defaults::default_l2c()}
      .add_parameter("mshr_size", static_cast<uint32_t>(8))
      .add_parameter("num_sets", static_cast<uint32_t>(1))
      .add_parameter("num_ways", static_cast<uint32_t>(1))
      .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul_seed.queues, &mock_ul_test.queues})
      .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
      .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
      .add_parameter("fill_latency", static_cast<uint64_t>(fill_latency))
      .add_parameter("pref_activate_mask", std::vector<access_type>{access_type::LOAD})
      .add_parameter("offset_bits", champsim::data::bits{})
      .add_submodule("replacement", champsim::modules::ModuleBuilder{"t442_update_state_collector_2", "update_state_collector"})
      .add_submodule("replacement", champsim::modules::ModuleBuilder{"t442_lru_2", "lru"})
    };

    std::array<champsim::operable*, 4> elements{{&mock_ll, &mock_ul_seed, &mock_ul_test, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    WHEN("A packet is issued")
    {
      uint64_t id = 0;
      decltype(mock_ul_seed)::request_type seed;
      seed.address = champsim::address{0xdeadbeef};
      seed.v_address = champsim::address{0xdeadbeef};
      seed.origin = champsim::origin{0, 0};
      seed.instr_id = id++;
      seed.type = access_type::WRITE;
      auto seed_result = mock_ul_seed.issue(seed);

      THEN("The issue is received") { REQUIRE(seed_result); }

      // Run the uut for a bunch of cycles to fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      AND_WHEN("A packet with a different address is issued")
      {
        ::replacement_cache_fill_collector[&uut].clear();

        decltype(mock_ul_test)::request_type test = seed;
        test.address = champsim::address{0xcafebabe};
        test.instr_id = id++;
        test.type = access_type::LOAD;
        auto test_result = mock_ul_test.issue(test);

        // Process the miss
        for (uint64_t i = 0; i < 100; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The issue is received")
        {
          REQUIRE(test_result);
          REQUIRE(mock_ll.packet_count() >= 1);
          REQUIRE(mock_ll.addresses.at(0) == test.address);
        }

        for (uint64_t i = 0; i < 2 * (fill_latency + hit_latency); ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("An eviction occurred")
        {
          REQUIRE_THAT(mock_ll.addresses, Catch::Matchers::SizeIs(2));
          REQUIRE(mock_ll.addresses.at(1) == seed.address);
        }

        THEN("The replacement policy is called with information from the evicted packet")
        {
          REQUIRE_THAT(::replacement_cache_fill_collector[&uut], Catch::Matchers::SizeIs(1));
          CHECK(::replacement_cache_fill_collector[&uut].back().cpu == test.origin.cpu());
          CHECK(::replacement_cache_fill_collector[&uut].back().set == 0);
          CHECK(::replacement_cache_fill_collector[&uut].back().way == 0);
          CHECK(::replacement_cache_fill_collector[&uut].back().full_addr == test.address);
          CHECK(::replacement_cache_fill_collector[&uut].back().type == access_type::LOAD);
          CHECK(::replacement_cache_fill_collector[&uut].back().victim_addr == seed.address);
        }
      }
    }
  }
}
