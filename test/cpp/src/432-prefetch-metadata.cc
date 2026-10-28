#include <catch.hpp>
#include <map>
#include <vector>

#include "cache.h"
#include "defaults.hpp"
#include "mocks.hpp"
#include "modules.h"

namespace test
{
std::map<champsim::modules::cache_module*, std::vector<uint32_t>> metadata_operate_collector;
std::map<champsim::modules::cache_module*, std::vector<uint32_t>> metadata_fill_collector;
} // namespace test

struct metadata_collector : champsim::modules::prefetcher {
  using prefetcher::prefetcher;

  champsim::modules::cache_module* parent_ = nullptr;

  void prefetcher_initialize() override {}
  uint32_t prefetcher_cache_operate(champsim::address, champsim::address, bool, bool, access_type, uint32_t metadata_in) override {
    auto it = test::metadata_operate_collector.try_emplace(parent_);
    it.first->second.push_back(metadata_in);
    return metadata_in;
  }

  uint32_t prefetcher_cache_fill(champsim::address, long, long, bool, champsim::address, uint32_t metadata_in) override
  {
    auto it = test::metadata_fill_collector.try_emplace(parent_);
    it.first->second.push_back(metadata_in);
    return metadata_in;
  }

  void prefetcher_cycle_operate() override {}
  void prefetcher_final_stats() override {}
  void prefetcher_branch_operate(champsim::address, uint8_t, champsim::address) override {}

  metadata_collector(champsim::modules::ModuleBuilder builder)
    : parent_(builder.get_parent<champsim::modules::cache_module>()) {}
};

template <uint32_t to_emit>
struct metadata_fill_emitter : champsim::modules::prefetcher {
  using prefetcher::prefetcher;

  void prefetcher_initialize() override {}
  uint32_t prefetcher_cache_operate(champsim::address, champsim::address, bool, bool, access_type, uint32_t metadata_in) override { return metadata_in; }
  uint32_t prefetcher_cache_fill(champsim::address, long, long, bool, champsim::address, uint32_t) override { return to_emit; }
  void prefetcher_cycle_operate() override {}
  void prefetcher_final_stats() override {}
  void prefetcher_branch_operate(champsim::address, uint8_t, champsim::address) override {}

  metadata_fill_emitter(champsim::modules::ModuleBuilder) {}
};
champsim::modules::prefetcher::register_module<metadata_collector> mc_register("metadata_collector");

SCENARIO("Prefetch metadata from an issued prefetch is seen in the lower level")
{
  GIVEN("An upper and lower level pair of caches")
  {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll;
    champsim::channel lower_queues{};
    CACHE lower{champsim::modules::ModuleBuilder{"t432_cache_0", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
      .add_parameter("mshr_size", static_cast<uint32_t>(8))
      .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&lower_queues})
      .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
      .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
      .add_parameter("fill_latency", static_cast<uint64_t>(fill_latency))
      .add_parameter("pref_activate_mask", std::vector<access_type>{access_type::PREFETCH})
      .add_submodule("prefetcher", champsim::modules::ModuleBuilder{"t432_metadata_collector_0", "metadata_collector"})
    };
    CACHE upper{champsim::modules::ModuleBuilder{"t432_cache_1", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
                    .add_parameter("num_sets", static_cast<uint32_t>(64))
                    .add_parameter("mshr_size", static_cast<uint32_t>(1))
                    .add_parameter("max_tag_bandwidth", champsim::bandwidth::maximum_type{1})
                    .add_parameter("max_fill_bandwidth", champsim::bandwidth::maximum_type{1})
                    .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&lower_queues))
                    .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
                    .add_parameter("fill_latency", static_cast<uint64_t>(fill_latency))};

    std::array<champsim::operable*, 3> elements{{&mock_ll, &lower, &upper}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    WHEN("The upper level issues a prefetch with metadata")
    {
      test::metadata_operate_collector.insert_or_assign(&lower, std::vector<uint32_t>{});

      // Request a prefetch
      champsim::address seed_addr{0xdeadbeef};
      constexpr uint32_t seed_metadata = 0xcafebabe;
      auto seed_result = upper.prefetch_line(seed_addr, true, seed_metadata);
      REQUIRE(seed_result);

      // Run the uut for a bunch of cycles to clear it out of the PQ and fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The lower level sees the metadata in prefetcher_cache_operate()")
      {
        REQUIRE_THAT(test::metadata_operate_collector.at(&lower), Catch::Matchers::Contains(seed_metadata));
      }
    }
  }
}

SCENARIO("Prefetch metadata from an filled block is seen in the upper level")
{
  GIVEN("An upper and lower level pair of caches")
  {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    constexpr uint32_t seed_metadata = 0xcafebabe;
    do_nothing_MRC mock_ll;
    champsim::channel lower_queues{};
    to_rq_MRP mock_ul;
    champsim::modules::prefetcher::register_module<metadata_fill_emitter<seed_metadata>> mfe_register("metadata_fill_emitter_seed_1");
    CACHE lower{champsim::modules::ModuleBuilder{"t432_cache_2", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
      .add_parameter("mshr_size", static_cast<uint32_t>(8))
      .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&lower_queues})
      .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
      .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
      .add_parameter("fill_latency", static_cast<uint64_t>(fill_latency))
      .add_submodule("prefetcher", champsim::modules::ModuleBuilder{"t432_metadata_fill_emitter_seed_1", "metadata_fill_emitter_seed_1"})
    };

    CACHE upper{champsim::modules::ModuleBuilder{"t432_cache_3", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
      .add_parameter("mshr_size", static_cast<uint32_t>(8))
      .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
      .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&lower_queues))
      .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
      .add_parameter("fill_latency", static_cast<uint64_t>(fill_latency))
      .add_submodule("prefetcher", champsim::modules::ModuleBuilder{"t432_metadata_collector_1", "metadata_collector"})
    };

    std::array<champsim::operable*, 4> elements{{&mock_ll, &lower, &upper, &mock_ul}};

    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    WHEN("The upper level experiences a miss and the lower level emits metadata on the fill")
    {
      champsim::address seed_addr{0xdeadbeef};

      test::metadata_fill_collector.insert_or_assign(&upper, std::vector<uint32_t>{});

      decltype(mock_ul)::request_type seed;
      seed.address = seed_addr;
      seed.origin = champsim::origin{0, 0};
      auto seed_result = mock_ul.issue(seed);
      REQUIRE(seed_result);

      // Run the uut for a bunch of cycles to clear it out of the PQ and fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The upper level sees the metadata in prefetcher_cache_fill()")
      {
        REQUIRE_THAT(test::metadata_fill_collector.at(&upper), Catch::Matchers::RangeEquals(std::vector{seed_metadata}));
      }
    }
  }
}
