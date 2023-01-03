#include "catch.hpp"
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

#include <map>
#include <vector>

namespace test
{
  extern std::map<CACHE*, std::vector<uint64_t>> address_operate_collector;
}

SCENARIO("A prefetch does not trigger itself") {
  GIVEN("A single cache") {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll;
    CACHE uut{"423-uut-0", 1, 1, 8, 32, hit_latency, fill_latency, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), {}, nullptr, &mock_ll.queues, CACHE::ptestDmodulesDprefetcherDaddress_collector, CACHE::rreplacementDlru};

    std::array<champsim::operable*, 2> elements{{&mock_ll, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A prefetch is issued") {
      test::address_operate_collector.insert_or_assign(&uut, std::vector<uint64_t>{});

      // Request a prefetch
      constexpr uint64_t seed_addr = 0xdeadbeef;
      auto seed_result = uut.prefetch_line(seed_addr, true, 0);

      THEN("The prefetch is issued") {
        REQUIRE(seed_result);
      }

      // Run the uut for a bunch of cycles to clear it out of the PQ and fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The prefetcher is not called") {
        REQUIRE(std::empty(test::address_operate_collector[&uut]));
      }
    }
  }
}

SCENARIO("The prefetcher is triggered if the packet matches the activate field") {
  using namespace std::literals;
  auto [type, str] = GENERATE(table<uint8_t, std::string_view>({std::pair{LOAD, "load"sv}, std::pair{RFO, "RFO"sv}, std::pair{PREFETCH, "prefetch"sv}, std::pair{WRITE, "write"sv}, std::pair{TRANSLATION, "translation"sv}}));
  GIVEN("A single cache") {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{"423-uut-1", 1, 1, 8, 32, hit_latency, fill_latency, 1, 1, 0, false, false, false, (1u<<type), {&mock_ul.queues}, nullptr, &mock_ll.queues, CACHE::ptestDmodulesDprefetcherDaddress_collector, CACHE::rreplacementDlru};

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A " + std::string{str} + " is issued") {
      test::address_operate_collector.insert_or_assign(&uut, std::vector<uint64_t>{});

      decltype(mock_ul)::request_type test;
      test.address = 0xdeadbeef;
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

      THEN("The prefetcher is called exactly once") {
        REQUIRE(std::size(test::address_operate_collector[&uut]) == 1);
      }

      THEN("The prefetcher is called with the issued address") {
        REQUIRE(test::address_operate_collector[&uut].front() == test.address);
      }
    }
  }
}

SCENARIO("The prefetcher is not triggered if the packet does not match the activate field") {
  using namespace std::literals;
  auto [type, str] = GENERATE(table<uint8_t, std::string_view>({std::pair{LOAD, "load"sv}, std::pair{RFO, "RFO"sv}, std::pair{PREFETCH, "prefetch"sv}, std::pair{WRITE, "write"sv}, std::pair{TRANSLATION, "translation"sv}}));
  GIVEN("A single cache") {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    auto mask = ((1u<<LOAD) | (1u<<RFO) | (1u<<PREFETCH) | (1u<<WRITE) | (1u<<TRANSLATION)) & ~(1u<<type);
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{"423-uut-2", 1, 1, 8, 32, hit_latency, fill_latency, 1, 1, 0, false, false, false, mask, {&mock_ul.queues}, nullptr, &mock_ll.queues, CACHE::ptestDmodulesDprefetcherDaddress_collector, CACHE::rreplacementDlru};

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A " + std::string{str} + " is issued") {
      test::address_operate_collector[&uut].clear();

      decltype(mock_ul)::request_type test;
      test.address = 0xdeadbeef;
      test.cpu = 0;
      test.type = type;
      auto test_result = mock_ul.issue(test);
      CHECK(test_result);

      // Run the uut for a bunch of cycles to fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The prefetcher is not called") {
        REQUIRE(std::empty(test::address_operate_collector[&uut]));
      }
    }
  }
}

