#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"

#include "cache.h"

struct merge_testbed
{
  constexpr static uint64_t hit_latency = 5;
  champsim::address address_that_will_hit{0xcafebabe};
  filter_MRC mock_ll{address_that_will_hit};
  to_rq_MRP seed_ul, test_ul;
  CACHE uut{champsim::cache_builder{champsim::defaults::default_l1d}
    .name("431-uut")
    .upper_levels({{&seed_ul.queues, &test_ul.queues}})
    .lower_level(&mock_ll.queues)
    .hit_latency(hit_latency)
  };
  uint32_t pkt_id = 0;

  std::array<champsim::operable*, 4> elements{{&mock_ll, &uut, &seed_ul, &test_ul}};

  template <typename MRP>
  void issue_type(MRP& ul, access_type type, uint64_t delay = hit_latency+1)
  {
    typename MRP::request_type pkt;
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

  explicit merge_testbed(access_type type)
  {
    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    issue_type(seed_ul, type);
  }

  void issue_type(access_type type, uint64_t delay = hit_latency+1)
  {
    issue_type(test_ul, type, delay);
  }
};

SCENARIO("A prefetch that hits an MSHR is dropped") {
  using namespace std::literals;
  auto [type, str] = GENERATE(table<access_type, std::string_view>({std::pair{access_type::LOAD, "load"sv}, std::pair{access_type::RFO, "RFO"sv}, std::pair{access_type::WRITE, "write"sv}, std::pair{access_type::TRANSLATION, "translation"sv}}));
  GIVEN("A cache with a " + std::string{str} + " miss") {
    merge_testbed testbed{type};

    THEN("An MSHR is created") {
      REQUIRE_THAT(testbed.uut.MSHR, Catch::Matchers::SizeIs(1));
      CHECK(testbed.uut.MSHR.front().instr_id == 0);
      CHECK_THAT(testbed.uut.MSHR.front().to_return, Catch::Matchers::SizeIs(1));
    }

    WHEN("A prefetch is issued") {
      testbed.issue_type(access_type::PREFETCH);

      THEN("The " + std::string{str} + " is in the MSHR") {
        REQUIRE_THAT(testbed.uut.MSHR, Catch::Matchers::SizeIs(1));
        CHECK(testbed.uut.MSHR.front().instr_id == 0);
        CHECK_THAT(testbed.uut.MSHR.front().to_return, Catch::Matchers::SizeIs(2));
      }
    }
  }
}

SCENARIO("A prefetch MSHR that gets hit is promoted") {
  using namespace std::literals;
  auto [type, str] = GENERATE(table<access_type, std::string_view>({std::pair{access_type::LOAD, "load"sv}, std::pair{access_type::RFO, "RFO"sv}, std::pair{access_type::WRITE, "write"sv}, std::pair{access_type::TRANSLATION, "translation"sv}}));
  GIVEN("A cache with a prefetch miss") {
    merge_testbed testbed{access_type::PREFETCH};

    THEN("An MSHR is created") {
      REQUIRE_THAT(testbed.uut.MSHR, Catch::Matchers::SizeIs(1));
      CHECK(testbed.uut.MSHR.front().instr_id == 0);
      CHECK_THAT(testbed.uut.MSHR.front().to_return, Catch::Matchers::SizeIs(1));
    }

    WHEN("A " + std::string{str} + " is issued") {
      auto old_time_enqueued = testbed.uut.MSHR.front().time_enqueued;

      testbed.issue_type(type);

      THEN("The " + std::string{str} + " is in the MSHR") {
        REQUIRE_THAT(testbed.uut.MSHR, Catch::Matchers::SizeIs(1));
        CHECK(testbed.uut.MSHR.front().time_enqueued > old_time_enqueued);
        //CHECK(testbed.uut.MSHR.front().instr_id == 1);
        CHECK_THAT(testbed.uut.MSHR.front().to_return, Catch::Matchers::SizeIs(2));
      }

      AND_WHEN("The MSHR is closed") {
        champsim::channel::response_type response{testbed.uut.MSHR.front().address, testbed.uut.MSHR.front().v_address, testbed.uut.MSHR.front().data_promise->data, 0, testbed.uut.MSHR.front().instr_depend_on_me};

        testbed.uut.lower_level->returned.push_back(response);
        for (uint64_t i = 0; i < 8*(testbed.hit_latency); ++i)
          for (auto elem : testbed.elements)
            elem->_operate();
        
        THEN("The average miss latency is reduced") {
          REQUIRE(std::llabs(testbed.uut.sim_stats.total_miss_latency_cycles) < testbed.hit_latency);
        }
      }
    }
  }
}

