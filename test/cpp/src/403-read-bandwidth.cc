#include <catch.hpp>

#include "cache.h"
#include "defaults.hpp"
#include "mocks.hpp"

TEMPLATE_TEST_CASE("The read queue respects the tag bandwidth", "", to_rq_MRP, to_wq_MRP, to_pq_MRP)
{
  constexpr auto hit_latency = 4;
  constexpr auto fill_latency = 1;
  constexpr auto tag_bandwidth = 2;

  auto size = GENERATE(range<long>(1, 4 * tag_bandwidth));

  GIVEN("A cache with a few elements")
  {
    do_nothing_MRC mock_ll;
    TestType mock_ul;
    CACHE uut{champsim::modules::ModuleBuilder{"t403_cache", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
                  .add_parameter("mshr_size", static_cast<uint32_t>(8))
                  .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                  .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                  .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
                  .add_parameter("fill_latency", static_cast<uint64_t>(fill_latency))
                  .add_parameter("max_tag_bandwidth", champsim::bandwidth::maximum_type{tag_bandwidth})
                  .add_parameter("max_fill_bandwidth", champsim::bandwidth::maximum_type{10})};

    std::array<champsim::operable*, 3> elements{{&uut, &mock_ll, &mock_ul}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    // Get a list of packets
    champsim::block_number seed_base_addr{0xdeadbeef};
    std::vector<typename TestType::request_type> seeds;

    for (auto i = 0; i < size; ++i) {
      typename TestType::request_type seed;
      seed.address = champsim::address{seed_base_addr + i};
      seed.instr_id = (uint64_t)i;
      seed.origin = champsim::origin{0, 0};

      seeds.push_back(seed);
    }
    REQUIRE(seeds.back().address == champsim::address{seed_base_addr + (size - 1)});

    for (auto& seed : seeds) {
      auto seed_result = mock_ul.issue(seed);
      REQUIRE(seed_result);
    }

    // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
    for (auto i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("The same packets are sent")
    {
      for (auto& pkt : seeds) {
        pkt.instr_id += 100;
      }

      for (auto& pkt : seeds) {
        auto test_result = mock_ul.issue(pkt);
        REQUIRE(test_result);
      }

      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      auto cycle = (size - 1) / tag_bandwidth;

      THEN("Packet " + std::to_string(size - 1) + " was served in cycle " + std::to_string(cycle))
      {
        REQUIRE_THAT(mock_ul.packets.back(), champsim::test::ReturnedMatcher(hit_latency + cycle, 1));
      }
    }
  }
}
