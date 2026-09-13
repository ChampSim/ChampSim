#include <catch.hpp>

#include "cache.h"
#include "defaults.hpp"
#include "mocks.hpp"

TEMPLATE_TEST_CASE("Translation misses do not inhibit other packets from being issued", "", to_wq_MRP, to_rq_MRP, to_pq_MRP)
{
  GIVEN("An empty cache")
  {
    constexpr static uint64_t hit_latency = 5;
    const champsim::address address_that_will_hit{0xcafebabe};
    filter_MRC mock_translator{address_that_will_hit};
    do_nothing_MRC mock_ll;
    TestType mock_ul{[](auto x, auto y) {
      return x.v_address == y.v_address;
    }};
    CACHE uut{champsim::modules::ModuleBuilder{"t413_cache", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
                  .add_parameter("mshr_size", static_cast<uint32_t>(8))
                  .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                  .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                  .add_parameter("lower_translate", static_cast<champsim::modules::channel_module*>(&mock_translator.queues))
                  .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
                  .add_parameter("fill_latency", static_cast<uint64_t>(3))};

    std::array<champsim::operable*, 4> elements{{&uut, &mock_ll, &mock_translator, &mock_ul}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    WHEN("A packet is issued that will miss the translator")
    {
      // Create a test packet
      typename TestType::request_type seed;
      seed.address = champsim::address{0xdeadbeef};
      seed.v_address = champsim::address{0xdeadbeef};
      seed.is_translated = false;
      seed.origin = champsim::origin{0, 0};

      mock_ul.issue(seed);

      // Operate enough cycles to realize we've missed
      for (uint64_t i = 0; i < (hit_latency + 1); ++i) {
        for (auto elem : elements)
          elem->_operate();
      }

      THEN("The packet has missed the translator")
      {
        REQUIRE(std::size(mock_ul.packets) == 1);
        REQUIRE(mock_ul.packets.front().return_time == 0);
      }

      AND_WHEN("An untranslated packet that will hit the translator is sent")
      {
        typename TestType::request_type test;
        test.address = address_that_will_hit;
        test.v_address = address_that_will_hit;
        seed.is_translated = false;
        test.origin = champsim::origin{0, 0};

        mock_ul.issue(test);

        // Operate long enough for the return to happen
        for (uint64_t i = 0; i < 100; ++i) {
          for (auto elem : elements)
            elem->_operate();
        }

        THEN("The second packet is issued")
        {
          REQUIRE(std::size(mock_ll.addresses) == 1);
          REQUIRE(mock_ll.addresses.front() == test.address);
        }
      }

      AND_WHEN("A translated packet is sent")
      {
        typename TestType::request_type test;
        test.address = champsim::address{0xfeedcafe};
        test.v_address = champsim::address{0xdeadbeef};
        test.is_translated = true;
        test.origin = champsim::origin{0, 0};

        mock_ul.issue(test);

        // Operate long enough for the return to happen
        for (uint64_t i = 0; i < 100; ++i) {
          for (auto elem : elements)
            elem->_operate();
        }

        THEN("The packet is translated")
        {
          REQUIRE(std::size(mock_ll.addresses) == 1);
          REQUIRE(mock_ll.addresses.front() == test.address);
        }
      }
    }
  }
}
