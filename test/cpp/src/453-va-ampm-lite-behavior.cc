#include <catch.hpp>
#include <numeric>
#include "mocks.hpp"
#include "matchers.hpp"
#include "cache.h"
#include "defaults.hpp"

#include "../../../prefetcher/va_ampm_lite/va_ampm_lite.h"

SCENARIO("The va_ampm_lite prefetcher issues prefetches when addresses stride in the positive direction") {
  auto stride = GENERATE(as<typename champsim::block_number::difference_type>{}, 1, 2, 3, -1, -2, -3);
  GIVEN("A cache with one filled block") {
    do_nothing_MRC mock_ll;
    do_nothing_MRC mock_lt;
    to_rq_MRP mock_ul{[](auto x, auto y){ return x.v_address == y.v_address; }};
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("453-uut-["+std::to_string(stride)+"]")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .lower_translate(&mock_lt.queues)
      .prefetcher<va_ampm_lite>()
    };

    std::array<champsim::operable*, 4> elements{{&mock_ll, &mock_lt, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    // Create a test packet
    static uint64_t id = 1;
    decltype(mock_ul)::request_type seed;
    seed.address = champsim::address{0xdeadbeef0800};
    seed.v_address = seed.address;
    seed.ip = champsim::address{0xcafecafe};
    seed.instr_id = id++;
    seed.cpu = 0;
    seed.is_translated = false;

    // Issue it to the uut
    auto seed_result = mock_ul.issue(seed);
    THEN("The issue is accepted") {
      REQUIRE(seed_result);
    }

    // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
    for (auto i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("Three more packets with the same IP but strided address is sent") {
      auto test_a = seed;
      test_a.address = champsim::address{champsim::block_number{seed.address} + stride};
      test_a.v_address = test_a.address;
      test_a.instr_id = id++;

      auto test_result_a = mock_ul.issue(test_a);
      THEN("The first issue is accepted") {
        REQUIRE(test_result_a);
      }

      auto test_b = test_a;
      test_b.address = champsim::address{champsim::block_number{test_a.address} + stride};
      test_b.v_address = test_b.address;
      test_b.instr_id = id++;

      auto test_result_b = mock_ul.issue(test_b);
      THEN("The second issue is accepted") {
        REQUIRE(test_result_b);
      }

      for (uint64_t i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      auto test_c = test_b;
      test_c.address = champsim::address{champsim::block_number{test_b.address} + stride};
      test_c.v_address = test_c.address;
      test_c.instr_id = id++;

      auto test_result_c = mock_ul.issue(test_c);
      THEN("The second issue is accepted") {
        REQUIRE(test_result_c);
      }

      for (uint64_t i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("A total of 5 requests were generated with the same stride") {
        REQUIRE_THAT(mock_ll.addresses, Catch::Matchers::SizeIs(5) && champsim::test::StrideMatcher<champsim::block_number>{static_cast<int64_t>(stride)});
      }
    }
  }
}

TEST_CASE("va_ampm_lite benchmark") {
  BENCHMARK_ADVANCED("va_ampm_lite::prefetcher_initialize()")(Catch::Benchmark::Chronometer meter){
    do_nothing_MRC mock_ll;
    do_nothing_MRC mock_lt;
    to_rq_MRP mock_ul{[](auto x, auto y){ return x.v_address == y.v_address; }};
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("453-uut-benchmark[va_ampm_lite::prefetcher_initialize()]")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .lower_translate(&mock_lt.queues)
      .prefetcher<va_ampm_lite>()
    };
    meter.measure([&] { return uut.impl_prefetcher_initialize(); });

  
  };

  BENCHMARK_ADVANCED("va_ampm_lite::prefetcher_cache_operate()")(Catch::Benchmark::Chronometer meter){
    do_nothing_MRC mock_ll;
    do_nothing_MRC mock_lt;
    to_rq_MRP mock_ul{[](auto x, auto y){ return x.v_address == y.v_address; }};
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("453-uut-benchmark[va_ampm_lite::prefetcher_cache_operate()]")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .lower_translate(&mock_lt.queues)
      .prefetcher<va_ampm_lite>()
    };
    meter.measure([&] { return uut.impl_prefetcher_cache_operate(champsim::address{}, champsim::address{}, false, false, access_type::LOAD,uint32_t{}); });
  };

  BENCHMARK_ADVANCED("va_ampm_lite::prefetcher_cycle_operate()")(Catch::Benchmark::Chronometer meter){
    do_nothing_MRC mock_ll;
    do_nothing_MRC mock_lt;
    to_rq_MRP mock_ul{[](auto x, auto y){ return x.v_address == y.v_address; }};
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("453-uut-benchmark[va_ampm_lite::prefetcher_cycle_operate()]")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .lower_translate(&mock_lt.queues)
      .prefetcher<va_ampm_lite>()
    };
    meter.measure([&] { return uut.impl_prefetcher_cycle_operate(); });
  };

  BENCHMARK_ADVANCED("va_ampm_lite::prefetcher_cache_fill()")(Catch::Benchmark::Chronometer meter){
    do_nothing_MRC mock_ll;
    do_nothing_MRC mock_lt;
    to_rq_MRP mock_ul{[](auto x, auto y){ return x.v_address == y.v_address; }};
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("453-uut-benchmark[va_ampm_lite::prefetcher_cache_fill()]")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .lower_translate(&mock_lt.queues)
      .prefetcher<va_ampm_lite>()
    };
    meter.measure([&] { return uut.impl_prefetcher_cache_fill(champsim::address{}, long{}, long{}, uint8_t{}, champsim::address{}, uint32_t{}); });
  };

  BENCHMARK_ADVANCED("va_ampm_lite::prefetcher_branch_operate()")(Catch::Benchmark::Chronometer meter){
    do_nothing_MRC mock_ll;
    do_nothing_MRC mock_lt;
    to_rq_MRP mock_ul{[](auto x, auto y){ return x.v_address == y.v_address; }};
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("453-uut-benchmark[va_ampm_lite::prefetcher_branch_operate()]")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .lower_translate(&mock_lt.queues)
      .prefetcher<va_ampm_lite>()
    };
    meter.measure([&] { return uut.impl_prefetcher_branch_operate(champsim::address{}, uint8_t{}, champsim::address{}); });
  };

  BENCHMARK_ADVANCED("va_ampm_lite::prefetcher_final_stats()")(Catch::Benchmark::Chronometer meter){
    do_nothing_MRC mock_ll;
    do_nothing_MRC mock_lt;
    to_rq_MRP mock_ul{[](auto x, auto y){ return x.v_address == y.v_address; }};
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("453-uut-benchmark[va_ampm_lite::prefetcher_final_stats()]")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .lower_translate(&mock_lt.queues)
      .prefetcher<va_ampm_lite>()
    };
    meter.measure([&] { return uut.impl_prefetcher_final_stats(); });
  };
  SUCCEED();
}
