#include <catch.hpp>
#include <map>
#include <vector>

#include "cache.h"
#include "defaults.hpp"
#include "mocks.hpp"
#include "modules.h"
namespace
{
std::map<champsim::modules::cache_module*, int> victim_interface_discerner;
std::map<champsim::modules::cache_module*, int> update_interface_discerner;
std::map<champsim::modules::cache_module*, int> fill_override_interface_discerner;

struct dual_interface : champsim::modules::replacement {
  using replacement::replacement;

    champsim::modules::cache_module* parent_ = nullptr;

    dual_interface(CACHE* c) {(void)c;}

    void initialize_replacement() override {}
    long find_victim(champsim::origin, uint64_t, long, const CACHE::BLOCK*, champsim::address, champsim::address, access_type) override
    {
      ::victim_interface_discerner[parent_] = 1;
      return 0;
    }

    void update_replacement_state(champsim::origin, long, long, champsim::address, champsim::address, champsim::address, access_type, bool) override
    {
      ::update_interface_discerner[parent_] = 1;
    }

    void replacement_cache_fill(champsim::origin, long, long, champsim::address, champsim::address, champsim::address, access_type) override {}
    void replacement_final_stats() override {}

    dual_interface(champsim::modules::ModuleBuilder builder)
      : parent_(builder.get_parent<champsim::modules::cache_module>()) {}
  };

  struct fill_selection : champsim::modules::replacement
  {
    using replacement::replacement;

    champsim::modules::cache_module* parent_ = nullptr;

    fill_selection(CACHE* c) {(void)c;}

    void initialize_replacement() override {}
    long find_victim(champsim::origin, uint64_t, long, const CACHE::BLOCK*, champsim::address, champsim::address, access_type) override
    {
      return 0;
    }

    void update_replacement_state(champsim::origin, long, long, champsim::address, champsim::address, champsim::address, access_type, bool) override
    {
      ::update_interface_discerner[parent_] = 1;
    }

    void replacement_cache_fill(champsim::origin, long, long, champsim::address, champsim::address, champsim::address, access_type) override
    {
      ::fill_override_interface_discerner[parent_] = 1;
    }

    void replacement_final_stats() override {}

    fill_selection(champsim::modules::ModuleBuilder builder)
      : parent_(builder.get_parent<champsim::modules::cache_module>()) {}
  };
}

champsim::modules::replacement::register_module<dual_interface> dual_interface_register("dual_interface");
champsim::modules::replacement::register_module<fill_selection> fill_selection_register("fill_selection");

SCENARIO("The simulator selects the address-based victim finder in replacement policies")
{
  using namespace std::literals;
  GIVEN("A single cache")
  {
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::modules::ModuleBuilder{"t443_cache_0", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
      .add_parameter("mshr_size", static_cast<uint32_t>(8))
      .add_parameter("num_sets", static_cast<uint32_t>(1))
      .add_parameter("num_ways", static_cast<uint32_t>(1))
      .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
      .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
      .add_parameter("offset_bits", champsim::data::bits{})
      .add_submodule("replacement", champsim::modules::ModuleBuilder{"t443_dual_interface_0", "dual_interface"})
      .add_submodule("replacement", champsim::modules::ModuleBuilder{"t443_lru_0", "lru"})
    };

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    WHEN("A packet is issued")
    {
      ::victim_interface_discerner.insert_or_assign(&uut, 0);

      decltype(mock_ul)::request_type seed;
      seed.address = champsim::address{0xdeadbeef};
      seed.is_translated = true;
      seed.origin = champsim::origin{0, 0};
      auto seed_result = mock_ul.issue(seed);

      THEN("The issue is received") { REQUIRE(seed_result); }

      // Run the uut for a bunch of cycles to fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      AND_WHEN("The packet is returned")
      {
        // Run the uut for a bunch of cycles to fill the cache
        for (auto i = 0; i < 100; ++i)
          for (auto elem : elements)
            elem->_operate();

        AND_WHEN("A packet with a different address is sent")
        {
          decltype(mock_ul)::request_type test;
          test.address = champsim::address{0xcafebabe};
          test.is_translated = true;
          test.origin = champsim::origin{0, 0};
          auto test_result = mock_ul.issue(test);

          THEN("The issue is received") { REQUIRE(test_result); }

          // Run the uut for a bunch of cycles to fill the cache
          for (auto i = 0; i < 100; ++i)
            for (auto elem : elements)
              elem->_operate();

          THEN("The replacement policy is called with address interface") { REQUIRE(::victim_interface_discerner[&uut] == 1); }
        }
      }
    }
  }
}

SCENARIO("The simulator selects the address-based update function in replacement policies")
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
    CACHE uut{champsim::modules::ModuleBuilder{"t443_cache_1", "DEFAULT_CACHE", champsim::defaults::default_l2c()}
      .add_parameter("mshr_size", static_cast<uint32_t>(8))
      .add_parameter("num_sets", static_cast<uint32_t>(1))
      .add_parameter("num_ways", static_cast<uint32_t>(1))
      .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
      .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
      .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
      .add_parameter("fill_latency", static_cast<uint64_t>(fill_latency))
      .add_parameter("pref_activate_mask", std::vector<access_type>{type})
      .add_parameter("offset_bits", champsim::data::bits{})
      .add_submodule("replacement", champsim::modules::ModuleBuilder{"t443_dual_interface_1", "dual_interface"})
      .add_submodule("replacement", champsim::modules::ModuleBuilder{"t443_lru_1", "lru"})
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
      ::update_interface_discerner[&uut] = 0;
      auto repeat_test_result = mock_ul.issue(test);

      THEN("The issue is received") { REQUIRE(repeat_test_result); }

      // Run the uut for a bunch of cycles to fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The replacement policy is called with the address interface") { REQUIRE(::update_interface_discerner[&uut] == 1); }
    }
  }
}

SCENARIO("The simulator selects the cache fill function if it is available")
{
  using namespace std::literals;
  auto [type, str] = GENERATE(table<access_type, std::string_view>({std::pair{access_type::LOAD, "load"sv}, std::pair{access_type::RFO, "RFO"sv},
                                                                    std::pair{access_type::PREFETCH, "prefetch"sv}, std::pair{access_type::WRITE, "write"sv},
                                                                    std::pair{access_type::TRANSLATION, "translation"sv}}));
  GIVEN("A cache with one element")
  {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 10;
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::modules::ModuleBuilder{"t443_cache_2", "DEFAULT_CACHE", champsim::defaults::default_l2c()}
      .add_parameter("mshr_size", static_cast<uint32_t>(8))
      .add_parameter("num_sets", static_cast<uint32_t>(1))
      .add_parameter("num_ways", static_cast<uint32_t>(1))
      .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
      .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
      .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
      .add_parameter("fill_latency", static_cast<uint64_t>(fill_latency))
      .add_parameter("pref_activate_mask", std::vector<access_type>{type})
      .add_parameter("offset_bits", champsim::data::bits{})
      .add_submodule("replacement", champsim::modules::ModuleBuilder{"t443_fill_selection", "fill_selection"})
      .add_submodule("replacement", champsim::modules::ModuleBuilder{"t443_lru_2", "lru"})
    };

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    WHEN("A packet with the is issued")
    {
      ::update_interface_discerner[&uut] = 0;
      ::fill_override_interface_discerner[&uut] = 0;
      decltype(mock_ul)::request_type test;
      test.address = champsim::address{0xdeadbeef};
      test.address = champsim::address{0xdeadbeef};
      test.origin = champsim::origin{0, 0};
      test.type = type;
      auto test_result = mock_ul.issue(test);

      THEN("The issue is received") { REQUIRE(test_result); }

      // Run the uut for a bunch of cycles to miss the cache
      for (uint64_t i = 0; i < 2 * hit_latency; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The replacement policy is called with the address interface")
      {
        REQUIRE(::update_interface_discerner[&uut] == 1);
        REQUIRE(::fill_override_interface_discerner[&uut] == 0);
      }

      AND_WHEN("The cache is filled")
      {
        // Run the uut for a bunch of cycles to fill the cache
        for (auto i = 0; i < 100; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The replacement policy is called with the address interface")
        {
          REQUIRE(::update_interface_discerner[&uut] == 1);
          REQUIRE(::fill_override_interface_discerner[&uut] == 1);
        }
      }
    }
  }
}
