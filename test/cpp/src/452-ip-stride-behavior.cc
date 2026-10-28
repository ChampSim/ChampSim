#include <algorithm>
#include <catch.hpp>
#include <numeric>

#include "../../../prefetcher/ip_stride/ip_stride.h"
#include "address.h"
#include "cache.h"
#include "defaults.hpp"
#include "mocks.hpp"

SCENARIO("The ip_stride prefetcher issues prefetches when the IP matches")
{
  auto stride = GENERATE(as<int64_t>{}, -4, -3, -2, -1, 1, 2, 3, 4);
  GIVEN("A cache with one filled block")
  {
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::modules::ModuleBuilder{"t452_cache", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
      .add_parameter("mshr_size", static_cast<uint32_t>(8))
      .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
      .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
      .add_submodule("prefetcher", champsim::modules::ModuleBuilder{"t452_ip_stride", "ip_stride"})
    };

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    // Create a test packet
    static uint64_t id = 1;
    decltype(mock_ul)::request_type seed;
    seed.address = champsim::address{0xffff'003f};
    seed.ip = champsim::address{0xcafecafe};
    seed.instr_id = id++;
    seed.origin = champsim::origin{0, 0};

    // Issue it to the uut
    auto seed_result = mock_ul.issue(seed);
    THEN("The issue is accepted") { REQUIRE(seed_result); }

    // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
    for (auto i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("Two more packets with the same IP but strided address is sent")
    {
      auto test_a = seed;
      test_a.address = champsim::address{champsim::block_number{seed.address} + stride};
      test_a.instr_id = id++;

      auto test_result_a = mock_ul.issue(test_a);
      THEN("The first issue is accepted") { REQUIRE(test_result_a); }

      auto test_b = test_a;
      test_b.address = champsim::address{champsim::block_number{test_a.address} + stride};
      test_b.instr_id = id++;

      auto test_result_b = mock_ul.issue(test_b);
      THEN("The second issue is accepted") { REQUIRE(test_result_b); }

      for (uint64_t i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("A total of 6 requests were generated with the same stride")
      {
        REQUIRE_THAT(mock_ll.addresses, Catch::Matchers::SizeIs(6) && champsim::test::StrideMatcher<champsim::block_number>{stride});
      }
    }
  }
}
