#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"
#include "modules.h"

#include <map>
#include <vector>

namespace
{
  std::map<CACHE*, std::vector<champsim::address>> address_operate_collector;

  struct address_collector : champsim::modules::prefetcher
  {
    using prefetcher::prefetcher;

    uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address, uint8_t, bool, access_type, uint32_t metadata_in)
    {
      ::address_operate_collector[intern_].push_back(addr);
      return metadata_in;
    }

    uint32_t prefetcher_cache_fill(champsim::address, long, long, uint8_t, champsim::address, uint32_t metadata_in)
    {
      return metadata_in;
    }
  };
}

SCENARIO("A prefetch does not trigger itself") {
  GIVEN("A single cache") {
    do_nothing_MRC mock_ll;
    CACHE uut{champsim::cache_builder{}
      .name("423a-uut")
      .sets(1)
      .ways(1)
      .lower_level(&mock_ll.queues)
      .prefetcher<::address_collector>()
    };

    std::array<champsim::operable*, 2> elements{{&mock_ll, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A prefetch is issued") {
      ::address_operate_collector.insert_or_assign(&uut, std::vector<champsim::address>{});

      // Request a prefetch
      champsim::address seed_addr{0xdeadbeef};
      auto seed_result = uut.prefetch_line(seed_addr, true, 0);

      THEN("The prefetch is issued") {
        REQUIRE(seed_result);
      }

      // Run the uut for a bunch of cycles to clear it out of the PQ and fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The prefetcher is not called") {
        REQUIRE(std::empty(::address_operate_collector[&uut]));
      }
    }
  }
}

SCENARIO("The prefetcher is triggered if the packet matches the activate field") {
  using namespace std::literals;
  auto [type, str] = GENERATE(table<access_type, std::string_view>({std::pair{access_type::LOAD, "load"sv}, std::pair{access_type::RFO, "RFO"sv}, std::pair{access_type::PREFETCH, "prefetch"sv}, std::pair{access_type::WRITE, "write"sv}, std::pair{access_type::TRANSLATION, "translation"sv}}));
  GIVEN("A single cache") {
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("423b-uut")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .prefetch_activate(type)
      .prefetcher<::address_collector>()
    };

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A " + std::string{str} + " is issued") {
      ::address_operate_collector.insert_or_assign(&uut, std::vector<champsim::address>{});

      decltype(mock_ul)::request_type test;
      test.address = champsim::address{0xdeadbeef};
      test.cpu = 0;
      test.type = type;
      auto test_result = mock_ul.issue(test);

      THEN("The issue is received") {
        REQUIRE(test_result);
      }

      // Run the uut for a bunch of cycles to fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The prefetcher is called exactly once with the issued address") {
        REQUIRE_THAT(::address_operate_collector.at(&uut), Catch::Matchers::SizeIs(1) && Catch::Matchers::Contains(test.address));
      }
    }
  }
}

SCENARIO("The prefetcher is not triggered if the packet does not match the activate field") {
  using namespace std::literals;
  auto [type, mask, str] = GENERATE(table< access_type, std::array<access_type, 4>, std::string_view >({
        std::tuple{access_type::LOAD, std::array{access_type::RFO, access_type::PREFETCH, access_type::WRITE, access_type::TRANSLATION}, "load"sv},
        std::tuple{access_type::RFO, std::array{access_type::LOAD, access_type::PREFETCH, access_type::WRITE, access_type::TRANSLATION}, "RFO"sv},
        std::tuple{access_type::PREFETCH, std::array{access_type::LOAD, access_type::RFO, access_type::WRITE, access_type::TRANSLATION}, "prefetch"sv},
        std::tuple{access_type::WRITE, std::array{access_type::LOAD, access_type::RFO, access_type::PREFETCH, access_type::TRANSLATION}, "write"sv},
        std::tuple{access_type::TRANSLATION, std::array{access_type::LOAD, access_type::RFO, access_type::PREFETCH, access_type::WRITE}, "translation"sv}
      }));

  GIVEN("A single cache") {
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;

    auto builder = champsim::cache_builder{champsim::defaults::default_l1d}
      .name("423c-uut")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .prefetcher<::address_collector>();

    builder = std::apply([&](auto... types){ return builder.prefetch_activate(types...); }, mask);

    CACHE uut{builder};

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A " + std::string{str} + " is issued") {
      ::address_operate_collector[&uut].clear();

      decltype(mock_ul)::request_type test;
      test.address = champsim::address{0xdeadbeef};
      test.cpu = 0;
      test.type = type;
      auto test_result = mock_ul.issue(test);
      CHECK(test_result);

      // Run the uut for a bunch of cycles to fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The prefetcher is not called") {
        REQUIRE(std::empty(::address_operate_collector[&uut]));
      }
    }
  }
}

