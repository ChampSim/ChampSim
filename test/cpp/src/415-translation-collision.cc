#include <catch.hpp>

#include "channel.h"
#include "defaults.hpp"
#include "mocks.hpp"

SCENARIO("A cache keeps the address for packets that don't need translation")
{
  GIVEN("An empty cache with one inflight translation")
  {
    constexpr uint64_t hit_latency = 10;
    do_nothing_MRC mock_translator;
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul{[](auto x, auto y) {
      return x.v_address == y.v_address;
    }};
    CACHE uut{champsim::modules::ModuleBuilder{"t415_cache", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
                  .add_parameter("mshr_size", static_cast<uint32_t>(8))
                  .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                  .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                  .add_parameter("lower_translate", static_cast<champsim::modules::channel_module*>(&mock_translator.queues))
                  .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
                  .add_parameter("fill_latency", static_cast<uint64_t>(3))
                  .add_parameter("max_tag_bandwidth", champsim::bandwidth::maximum_type{10})};

    std::array<champsim::operable*, 4> elements{{&uut, &mock_ll, &mock_ul, &mock_translator}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    // Create a test packet
    typename decltype(mock_ul)::request_type seed;
    seed.address = champsim::address{0xdeadbeef};
    seed.v_address = champsim::address{0xdeadbeef};
    seed.is_translated = false;
    seed.origin = champsim::origin{0, 0};

    mock_ul.issue(seed);

    // for (auto elem : elements)
    // elem->_operate();

    WHEN("A packet is sent that matches the seed's virtual page but doesn't need to be translated")
    {
      typename decltype(mock_ul)::request_type test;
      test.address = champsim::address{0xcafebabe};
      test.v_address = champsim::address{0xdeadb000};
      test.is_translated = true;
      test.origin = champsim::origin{0, 0};

      mock_ul.issue(test);

      for (int i = 0; i < 20; ++i) {
        for (auto elem : elements)
          elem->_operate();
      }

      THEN("The address of the test packet is unmodified")
      {
        REQUIRE_THAT(mock_ll.addresses, Catch::Matchers::UnorderedRangeEquals(std::vector{champsim::address{0x11111eef}, test.address}));
      }
    }
  }
}
