#include <catch.hpp>

#include <map>

#include "cache.h"
#include "defaults.hpp"
#include "mocks.hpp"

namespace
{
  std::map<CACHE*, int> operate_interface_discerner;
  std::map<CACHE*, int> fill_interface_discerner;

  struct dual_interface : champsim::modules::prefetcher
  {
    using prefetcher::prefetcher;

    uint32_t prefetcher_cache_operate(uint64_t, uint64_t, uint8_t, uint32_t, uint32_t metadata_in)
    {
      ::operate_interface_discerner[intern_] = 1;
      return metadata_in;
    }

    uint32_t prefetcher_cache_operate(champsim::address, champsim::address, uint8_t, bool, uint32_t, uint32_t metadata_in)
    {
      ::operate_interface_discerner[intern_] = 2;
      return metadata_in;
    }

    uint32_t prefetcher_cache_operate(champsim::address, champsim::address, uint8_t, bool, access_type, uint32_t metadata_in)
    {
      ::operate_interface_discerner[intern_] = 3;
      return metadata_in;
    }

    uint32_t prefetcher_cache_fill(uint64_t, long, long, uint8_t, uint64_t, uint32_t metadata_in)
    {
      ::fill_interface_discerner[intern_] = 1;
      return metadata_in;
    }

    uint32_t prefetcher_cache_fill(champsim::address, long, long, uint8_t, champsim::address, uint32_t metadata_in)
    {
      ::fill_interface_discerner[intern_] = 2;
      return metadata_in;
    }
  };
}

SCENARIO("The prefetcher interface prefers one that uses champsim::address") {
  using namespace std::literals;
  GIVEN("A single cache") {
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("430-uut")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .prefetcher<::dual_interface>()
    };

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A packet is issued") {
      ::operate_interface_discerner.insert_or_assign(&uut, 0);
      ::fill_interface_discerner.insert_or_assign(&uut, 0);

      decltype(mock_ul)::request_type test;
      test.address = champsim::address{0xdeadbeef};
      test.cpu = 0;
      auto test_result = mock_ul.issue(test);

      THEN("The issue is received") {
        REQUIRE(test_result);
      }

      // Run the uut for a bunch of cycles to fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The prefetcher operate hook is called") {
        REQUIRE(::operate_interface_discerner.at(&uut) == 3);
      }

      THEN("The prefetcher fill hook is called") {
        REQUIRE(::fill_interface_discerner.at(&uut) == 2);
      }
    }
  }
}
