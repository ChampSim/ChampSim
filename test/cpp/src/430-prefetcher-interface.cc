#include <catch.hpp>
#include <map>

#include "cache.h"
#include "defaults.hpp"
#include "mocks.hpp"

namespace
{
std::map<champsim::modules::cache_module*, int> operate_interface_discerner;
std::map<champsim::modules::cache_module*, int> fill_interface_discerner;

struct dual_interface : champsim::modules::prefetcher {
  using prefetcher::prefetcher;

    champsim::modules::cache_module* parent_ = nullptr;

    void prefetcher_initialize() override {}
    uint32_t prefetcher_cache_operate(champsim::address, champsim::address, bool, bool, access_type, uint32_t metadata_in) override
    {
      ::operate_interface_discerner[parent_] = 1;
      return metadata_in;
    }

    uint32_t prefetcher_cache_fill(champsim::address, long, long, bool, champsim::address, uint32_t metadata_in) override
    {
      ::fill_interface_discerner[parent_] = 1;
      return metadata_in;
    }

    void prefetcher_cycle_operate() override {}
    void prefetcher_final_stats() override {}
    void prefetcher_branch_operate(champsim::address, uint8_t, champsim::address) override {}

    dual_interface(champsim::modules::ModuleBuilder builder)
      : parent_(builder.get_parent<champsim::modules::cache_module>()) {}
  };

  champsim::modules::prefetcher::register_module<dual_interface> dual_interface_register("dual_interface_2");
}

SCENARIO("The prefetcher interface prefers one that uses champsim::address")
{
  using namespace std::literals;
  GIVEN("A single cache")
  {
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::modules::ModuleBuilder{"t430_cache", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
      .add_parameter("mshr_size", static_cast<uint32_t>(8))
      .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
      .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
      .add_submodule("prefetcher", champsim::modules::ModuleBuilder{"t430_dual_interface_2", "dual_interface_2"})
    };

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    WHEN("A packet is issued")
    {
      ::operate_interface_discerner.insert_or_assign(&uut, 0);
      ::fill_interface_discerner.insert_or_assign(&uut, 0);

      decltype(mock_ul)::request_type test;
      test.address = champsim::address{0xdeadbeef};
      test.origin = champsim::origin{0, 0};
      auto test_result = mock_ul.issue(test);

      THEN("The issue is received") { REQUIRE(test_result); }

      // Run the uut for a bunch of cycles to fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The prefetcher operate hook is called") { REQUIRE(::operate_interface_discerner.at(&uut) == 1); }

      THEN("The prefetcher fill hook is called") { REQUIRE(::fill_interface_discerner.at(&uut) == 1); }
    }
  }
}
