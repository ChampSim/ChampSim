#include "catch.hpp"
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"
#include "repl_interface.h"

#include <map>
#include <vector>

namespace test
{
  extern std::map<CACHE*, std::vector<repl_update_interface>> replacement_update_state_collector;
}

SCENARIO("The replacement policy is not triggered on a miss, but on a fill") {
  using namespace std::literals;
  auto [type, str] = GENERATE(table<uint8_t, std::string_view>({std::pair{LOAD, "load"sv}, std::pair{RFO, "RFO"sv}, std::pair{PREFETCH, "prefetch"sv}, std::pair{WRITE, "write"sv}, std::pair{TRANSLATION, "translation"sv}}));
  GIVEN("A single cache") {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    release_MRC mock_ll;
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"442-uut-1-"+std::string{str}, 1, 1, 8, 32, fill_latency, 1, 1, 0, false, false, false, (1u<<type), uut_queues, &mock_ll, CACHE::pprefetcherDno, CACHE::rtestDmodulesDreplacementDlru_collect};
    to_rq_MRP mock_ul{&uut};

    std::array<champsim::operable*, 4> elements{{&mock_ll, &mock_ul, &uut_queues, &uut}};

    // Initialize the prefetching and replacement
    uut.initialize();

    // Turn off warmup
    for (auto elem : elements) {
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A " + std::string{str} + " is issued") {
      test::replacement_update_state_collector[&uut].clear();

      PACKET test;
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

      THEN("The replacement policy is not called") {
        REQUIRE(std::size(test::replacement_update_state_collector[&uut]) == 0);
      }

      AND_WHEN("The packet is returned") {
        mock_ll.release(test.address);

        // Run the uut for a bunch of cycles to fill the cache
        for (auto i = 0; i < 100; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The replacement policy is called once") {
          REQUIRE(std::size(test::replacement_update_state_collector[&uut]) == 1);
        }

        THEN("The replacement policy is called with information from the issued packet") {
          CHECK(test::replacement_update_state_collector[&uut].front().cpu == test.cpu);
          CHECK(test::replacement_update_state_collector[&uut].front().set == 0);
          CHECK(test::replacement_update_state_collector[&uut].front().way == 0);
          CHECK(test::replacement_update_state_collector[&uut].front().full_addr == test.address);
          CHECK(test::replacement_update_state_collector[&uut].front().type == test.type);
          CHECK(test::replacement_update_state_collector[&uut].front().hit == false);
        }
      }
    }
  }
}

SCENARIO("The replacement policy is triggered on a hit") {
  using namespace std::literals;
  auto [type, str] = GENERATE(table<uint8_t, std::string_view>({std::pair{LOAD, "load"sv}, std::pair{RFO, "RFO"sv}, std::pair{PREFETCH, "prefetch"sv}, std::pair{WRITE, "write"sv}, std::pair{TRANSLATION, "translation"sv}}));
  GIVEN("A cache with one element") {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll;
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"442-uut-1-"+std::string{str}, 1, 1, 8, 32, fill_latency, 1, 1, 0, false, false, false, (1u<<type), uut_queues, &mock_ll, CACHE::pprefetcherDno, CACHE::rtestDmodulesDreplacementDlru_collect};
    to_rq_MRP mock_ul{&uut};

    std::array<champsim::operable*, 4> elements{{&mock_ll, &mock_ul, &uut_queues, &uut}};

    // Initialize the prefetching and replacement
    uut.initialize();

    // Turn off warmup
    for (auto elem : elements) {
      elem->warmup = false;
      elem->begin_phase();
    }

    PACKET test;
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

    WHEN("A packet with the same address is issued") {
      test::replacement_update_state_collector[&uut].clear();
      auto repeat_test_result = mock_ul.issue(test);

      THEN("The issue is received") {
        REQUIRE(repeat_test_result);
      }

      // Run the uut for a bunch of cycles to fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The replacement policy is called once") {
        REQUIRE(std::size(test::replacement_update_state_collector[&uut]) == 1);
      }

      THEN("The replacement policy is called with information from the issued packet") {
        CHECK(test::replacement_update_state_collector[&uut].front().cpu == test.cpu);
        CHECK(test::replacement_update_state_collector[&uut].front().set == 0);
        CHECK(test::replacement_update_state_collector[&uut].front().way == 0);
        CHECK(test::replacement_update_state_collector[&uut].front().full_addr == test.address);
        CHECK(test::replacement_update_state_collector[&uut].front().type == test.type);
        CHECK(test::replacement_update_state_collector[&uut].front().hit == true);
      }
    }
  }
}

SCENARIO("The replacement policy notes the correct eviction information") {
  GIVEN("An empty cache") {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll;
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"442-uut-3", 1, 1, 1, 32, fill_latency, 1, 1, 0, false, false, false, (1u<<LOAD), uut_queues, &mock_ll, CACHE::pprefetcherDno, CACHE::rtestDmodulesDreplacementDlru_collect};
    to_wq_MRP mock_ul_seed{&uut};
    to_rq_MRP mock_ul_test{&uut};

    std::array<champsim::operable*, 5> elements{{&mock_ll, &mock_ul_seed, &mock_ul_test, &uut_queues, &uut}};

    // Initialize the prefetching and replacement
    uut.initialize();

    // Turn off warmup
    for (auto elem : elements) {
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A packet is issued") {
      uint64_t id = 0;
      PACKET seed;
      seed.address = 0xdeadbeef;
      seed.v_address = 0xdeadbeef;
      seed.cpu = 0;
      seed.instr_id = id++;
      seed.type = WRITE;
      auto seed_result = mock_ul_seed.issue(seed);

      THEN("The issue is received") {
        REQUIRE(seed_result);
      }

      // Run the uut for a bunch of cycles to fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      AND_WHEN("A packet with a different address is issued") {

        PACKET test = seed;
        test.address = 0xcafebabe;
        test.instr_id = id++;
        test.type = LOAD;
        auto test_result = mock_ul_test.issue(test);

        // Process the miss
        for (uint64_t i = 0; i < hit_latency+1; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The issue is received") {
          CHECK(test_result);
          CHECK(std::size(uut.MSHR) == 1);
          CHECK(mock_ll.packet_count() == 1);
          CHECK(mock_ll.addresses.back() == test.address);
        }

        test::replacement_update_state_collector[&uut].clear();

        for (uint64_t i = 0; i < 2*(fill_latency+hit_latency); ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("An eviction occurred") {
          CHECK(mock_ll.packet_count() == 2);
          CHECK(mock_ll.addresses.back() == seed.address);
        }

        THEN("The replacement policy is called once") {
          REQUIRE(std::size(test::replacement_update_state_collector[&uut]) == 1);
        }

        THEN("The replacement policy is called with information from the evicted packet") {
          CHECK(test::replacement_update_state_collector[&uut].back().cpu == test.cpu);
          CHECK(test::replacement_update_state_collector[&uut].back().set == 0);
          CHECK(test::replacement_update_state_collector[&uut].back().way == 0);
          CHECK(test::replacement_update_state_collector[&uut].back().full_addr == test.address);
          CHECK(test::replacement_update_state_collector[&uut].back().type == LOAD);
          CHECK(test::replacement_update_state_collector[&uut].back().victim_addr == seed.address);
          CHECK(test::replacement_update_state_collector[&uut].back().hit == false);
        }
      }
    }
  }
}
