#include <catch.hpp>

#include "channel.h"
#include "defaults.hpp"
#include "mocks.hpp"

TEMPLATE_TEST_CASE("Caches issue translations", "", to_wq_MRP, to_rq_MRP, to_pq_MRP)
{
  GIVEN("An empty cache with a translator")
  {
    constexpr uint64_t hit_latency = 10;
    do_nothing_MRC mock_translator;
    do_nothing_MRC mock_ll;
    TestType mock_ul{[](auto x, auto y) {
      return x.v_address == y.v_address;
    }};
    CACHE uut{champsim::modules::ModuleBuilder{"t411_cache_0", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
                  .add_parameter("mshr_size", static_cast<uint32_t>(8))
                  .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                  .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                  .add_parameter("lower_translate", static_cast<champsim::modules::channel_module*>(&mock_translator.queues))
                  .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
                  .add_parameter("fill_latency", static_cast<uint64_t>(3))};

    std::array<champsim::operable*, 4> elements{{&uut, &mock_ll, &mock_ul, &mock_translator}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    WHEN("A packet is sent")
    {
      // Create a test packet
      typename TestType::request_type test;
      test.address = champsim::address{0xdeadbeef};
      test.v_address = champsim::address{0xdeadbeef};
      test.is_translated = false;
      test.origin = champsim::origin{0, 0};

      auto test_result = mock_ul.issue(test);
      THEN("The issue is accepted") { REQUIRE(test_result); }

      for (auto elem : elements)
        elem->_operate();

      THEN("The packet is issued for translation") { REQUIRE(mock_translator.packet_count() == 1); }

      for (int i = 0; i < 100; ++i) {
        for (auto elem : elements)
          elem->_operate();
      }

      THEN("The packet is translated") { REQUIRE_THAT(mock_ll.addresses, Catch::Matchers::RangeEquals(std::vector{champsim::address{0x11111eef}})); }
    }
  }
}

TEMPLATE_TEST_CASE("Translations work even if the addresses happen to be the same", "", to_wq_MRP, to_rq_MRP, to_pq_MRP)
{
  GIVEN("An empty cache with a translator")
  {
    constexpr uint64_t hit_latency = 10;
    release_MRC mock_translator; // release_MRC used because it does not manipulate the data field
    do_nothing_MRC mock_ll;
    TestType mock_ul{[](auto x, auto y) {
      return x.v_address == y.v_address;
    }};
    CACHE uut{champsim::modules::ModuleBuilder{"t411_cache_1", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
                  .add_parameter("mshr_size", static_cast<uint32_t>(8))
                  .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                  .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                  .add_parameter("lower_translate", static_cast<champsim::modules::channel_module*>(&mock_translator.queues))
                  .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
                  .add_parameter("fill_latency", static_cast<uint64_t>(3))};

    std::array<champsim::operable*, 4> elements{{&uut, &mock_ll, &mock_ul, &mock_translator}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    WHEN("A packet is sent")
    {
      // Create a test packet
      typename TestType::request_type test;
      test.address = champsim::address{0x11111eef};
      test.v_address = test.address;
      test.is_translated = false;
      test.origin = champsim::origin{0, 0};

      auto test_result = mock_ul.issue(test);
      THEN("The issue is accepted") { REQUIRE(test_result); }

      for (auto elem : elements)
        elem->_operate();

      THEN("The packet is issued for translation") { REQUIRE(mock_translator.packet_count() == 1); }

      mock_translator.release(test.address);
      for (int i = 0; i < 100; ++i) {
        for (auto elem : elements)
          elem->_operate();
      }

      THEN("The packet is translated") { REQUIRE_THAT(mock_ll.addresses, Catch::Matchers::RangeEquals(std::vector{champsim::address{0x11111eef}})); }
    }
  }
}
