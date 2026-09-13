#include <catch.hpp>

#include "cache.h"
#include "defaults.hpp"
#include "mocks.hpp"
#include "modules.h"

template <uint64_t bypass_addr>
struct bypass_replacement : champsim::modules::replacement {
  using replacement::replacement;

  bypass_replacement(CACHE* c) {(void)c;}

  void initialize_replacement() override {}
  long find_victim(champsim::origin, uint64_t, long, const champsim::cache_block*, champsim::address, champsim::address addr, access_type) override
  {
    if (addr == champsim::address{bypass_addr})
      return 1L;
    return 0L;
  }

  void update_replacement_state(champsim::origin, long, long, champsim::address, champsim::address, champsim::address, access_type, bool) override {}
  void replacement_cache_fill(champsim::origin, long, long, champsim::address, champsim::address, champsim::address, access_type) override {}
  void replacement_final_stats() override {}

  bypass_replacement(champsim::modules::ModuleBuilder) {}
};
champsim::modules::replacement::register_module<bypass_replacement<0xcafebabe>> bypass_replacement_register("bypass_replacement");
SCENARIO("The replacement policy can bypass") {
  using namespace std::literals;
  GIVEN("A single cache")
  {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll;
    to_wq_MRP mock_ul_seed;
    to_rq_MRP mock_ul_test;
    CACHE uut{champsim::modules::ModuleBuilder{"t441_cache", "DEFAULT_CACHE", champsim::defaults::default_l2c()}
      .add_parameter("mshr_size", static_cast<uint32_t>(8))
      .add_parameter("num_sets", static_cast<uint32_t>(1))
      .add_parameter("num_ways", static_cast<uint32_t>(1))
      .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul_seed.queues, &mock_ul_test.queues})
      .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
      .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
      .add_parameter("fill_latency", static_cast<uint64_t>(fill_latency))
      .add_parameter("offset_bits", champsim::data::bits{})
      .clear_submodules("replacement")
      .add_submodule("replacement", champsim::modules::ModuleBuilder{"t441_bypass_replacement", "bypass_replacement"})
    };

    std::array<champsim::operable*, 4> elements{{&mock_ll, &uut, &mock_ul_seed, &mock_ul_test}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    WHEN("A packet is issued")
    {
      decltype(mock_ul_seed)::request_type test;
      test.address = champsim::address{0xdeadbeef};
      test.origin = champsim::origin{0, 0};
      test.type = access_type::WRITE;
      auto test_result = mock_ul_seed.issue(test);

      THEN("The issue is received") { REQUIRE(test_result); }

      // Run the uut for a bunch of cycles to fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      AND_WHEN("A packet with a different address is sent")
      {
        decltype(mock_ul_test)::request_type test_b;
        test_b.address = champsim::address{0xcafebabe};
        test_b.origin = champsim::origin{0, 0};
        test_b.type = access_type::LOAD;
        test_b.instr_id = 1;

        auto test_b_result = mock_ul_test.issue(test_b);

        for (uint64_t i = 0; i < 2 * hit_latency; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The issue is received")
        {
          CHECK(test_b_result);
          CHECK_THAT(mock_ll.addresses, Catch::Matchers::RangeEquals(std::vector{test_b.address}));
        }

        for (uint64_t i = 0; i < 2 * (fill_latency + hit_latency); ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("No blocks are evicted") { REQUIRE_THAT(mock_ll.addresses, Catch::Matchers::RangeEquals(std::vector{test_b.address})); }
      }
    }
  }
}
