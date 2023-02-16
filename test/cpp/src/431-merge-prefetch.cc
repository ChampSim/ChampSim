#include "catch.hpp"
#include "mocks.hpp"

#include "cache.h"
#include "champsim_constants.h"

struct merge_testbed
{
  constexpr static uint64_t hit_latency = 5;
  champsim::address address_that_will_hit{0xcafebabe};
  filter_MRC mock_ll{address_that_will_hit};
  CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
  CACHE uut{"431-uut", 1, 1, 8, 32, 1, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &mock_ll, CACHE::pprefetcherDno, CACHE::rreplacementDlru};
  to_rq_MRP<CACHE> seed_ul{&uut};
  to_rq_MRP<CACHE> test_ul{&uut};
  uint32_t pkt_id = 0;

  std::array<champsim::operable*, 5> elements{{&mock_ll, &uut_queues, &uut, &seed_ul, &test_ul}};

  template <typename MRP>
  void issue_type(MRP& ul, uint8_t type, uint64_t delay = hit_latency+1)
  {
    PACKET pkt;
    pkt.address = champsim::address{0xdeadbeef};
    pkt.v_address = champsim::address{0xdeadbeef};
    pkt.type = type;
    pkt.instr_id = pkt_id++;
    pkt.cpu = 0;

    ul.issue(pkt);

    // Operate enough cycles to realize we've missed
    for (uint64_t i = 0; i < delay; ++i)
      for (auto elem : elements)
        elem->_operate();
  }

  explicit merge_testbed(uint8_t type)
  {
    // Initialize the prefetching and replacement
    uut.initialize();

    // Turn off warmup
    uut.warmup = false;
    uut_queues.warmup = false;
    uut.begin_phase();
    uut_queues.begin_phase();

    issue_type(seed_ul, type);
  }

  void issue_type(uint8_t type, uint64_t delay = hit_latency+1)
  {
    issue_type(test_ul, type, delay);
  }
};

SCENARIO("A prefetch that hits an MSHR is dropped") {
  using namespace std::literals;
  auto [type, str] = GENERATE(table<uint8_t, std::string_view>({std::pair{LOAD, "load"sv}, std::pair{RFO, "RFO"sv}, std::pair{WRITE, "write"sv}, std::pair{TRANSLATION, "translation"sv}}));
  GIVEN("A cache with a " + std::string{str} + " miss") {
    merge_testbed testbed{type};

    THEN("An MSHR is created") {
      REQUIRE(std::size(testbed.uut.MSHR) == 1);
      CHECK(testbed.uut.MSHR.front().instr_id == 0);
      CHECK(std::size(testbed.uut.MSHR.front().to_return) == 1);
    }

    WHEN("A prefetch is issued") {
      testbed.issue_type(PREFETCH);

      THEN("The " + std::string{str} + " is in the MSHR") {
        REQUIRE(std::size(testbed.uut.MSHR) == 1);
        CHECK(testbed.uut.MSHR.front().instr_id == 0);
        CHECK(std::size(testbed.uut.MSHR.front().to_return) == 2);
      }
    }
  }
}

SCENARIO("A prefetch MSHR that gets hit is promoted") {
  using namespace std::literals;
  auto [type, str] = GENERATE(table<uint8_t, std::string_view>({std::pair{LOAD, "load"sv}, std::pair{RFO, "RFO"sv}, std::pair{WRITE, "write"sv}, std::pair{TRANSLATION, "translation"sv}}));
  GIVEN("A cache with a prefetch miss") {
    merge_testbed testbed{PREFETCH};

    THEN("An MSHR is created") {
      REQUIRE(std::size(testbed.uut.MSHR) == 1);
      CHECK(testbed.uut.MSHR.front().instr_id == 0);
      CHECK(std::size(testbed.uut.MSHR.front().to_return) == 1);
    }

    WHEN("A " + std::string{str} + " is issued") {
      auto old_cycle_enqueued = testbed.uut.MSHR.front().cycle_enqueued;

      testbed.issue_type(type);

      THEN("The " + std::string{str} + " is in the MSHR") {
        REQUIRE(std::size(testbed.uut.MSHR) == 1);
        CHECK(testbed.uut.MSHR.front().cycle_enqueued > old_cycle_enqueued);
        //CHECK(testbed.uut.MSHR.front().instr_id == 1);
        CHECK(std::size(testbed.uut.MSHR.front().to_return) == 2);
      }
    }
  }
}

