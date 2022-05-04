#include "catch.hpp"
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

SCENARIO("Cache queues issue translations in WQ") {
  GIVEN("A write queue with one item") {
    constexpr uint64_t hit_latency = 1;
    do_nothing_MRC mock_ll;
    CACHE::TranslatingQueues uut{1, 32, 32, 32, hit_latency, LOG2_BLOCK_SIZE, false};
    uut.lower_level = &mock_ll;

    // Turn off warmup
    uut.warmup = false;
    uut.begin_phase();

    WHEN("A packet is sent") {
      // Create a test packet
      PACKET test;
      test.address = 0xdeadbeef;
      test.v_address = 0xdeadbeef;
      test.cpu = 0;

      auto test_result = uut.add_wq(test);
      REQUIRE(test_result);

      auto old_event_cycle = uut.current_cycle;

      mock_ll._operate();
      uut._operate();

      THEN("The packet is issued for translation") {
        REQUIRE(mock_ll.packet_count() == 1);
        REQUIRE(uut.WQ.front().address == 0);
        REQUIRE(uut.WQ.front().event_cycle == old_event_cycle + hit_latency);
      }

      mock_ll._operate();

      AND_THEN("The packet is translated") {
        REQUIRE(uut.WQ.front().address == 0x11111eef);
        REQUIRE(uut.WQ.front().v_address == test.v_address);
        REQUIRE(uut.wq_has_ready());
      }
    }
  }
}


SCENARIO("Cache queues issue translations in RQ") {
  GIVEN("A read queue with one item") {
    constexpr uint64_t hit_latency = 1;
    do_nothing_MRC mock_ll;
    CACHE::TranslatingQueues uut{1, 32, 32, 32, hit_latency, LOG2_BLOCK_SIZE, false};
    uut.lower_level = &mock_ll;

    // Turn off warmup
    uut.warmup = false;
    uut.begin_phase();

    WHEN("A packet is sent") {
      // Create a test packet
      PACKET test;
      test.address = 0xdeadbeef;
      test.v_address = 0xdeadbeef;
      test.cpu = 0;

      auto test_result = uut.add_rq(test);
      REQUIRE(test_result);

      auto old_event_cycle = uut.current_cycle;

      mock_ll._operate();
      uut._operate();

      THEN("The packet is issued for translation") {
        REQUIRE(mock_ll.packet_count() == 1);
        REQUIRE(uut.RQ.front().address == 0);
        REQUIRE(uut.RQ.front().event_cycle == old_event_cycle + hit_latency);
      }

      mock_ll._operate();

      AND_THEN("The packet is translated") {
        REQUIRE(uut.RQ.front().address == 0x11111eef);
        REQUIRE(uut.RQ.front().v_address == test.v_address);
        REQUIRE(uut.rq_has_ready());
      }
    }
  }
}


SCENARIO("Cache queues issue translations in PQ") {
  GIVEN("A prefetch queue with one item") {
    constexpr uint64_t hit_latency = 1;
    do_nothing_MRC mock_ll;
    CACHE::TranslatingQueues uut{1, 32, 32, 32, hit_latency, LOG2_BLOCK_SIZE, false};
    uut.lower_level = &mock_ll;

    // Turn off warmup
    uut.warmup = false;
    uut.begin_phase();

    WHEN("A packet is sent") {
      // Create a test packet
      PACKET test;
      test.address = 0xdeadbeef;
      test.v_address = 0xdeadbeef;
      test.cpu = 0;

      auto test_result = uut.add_pq(test);
      REQUIRE(test_result);

      auto old_event_cycle = uut.current_cycle;

      mock_ll._operate();
      uut._operate();

      THEN("The packet is issued for translation") {
        REQUIRE(mock_ll.packet_count() == 1);
        REQUIRE(uut.PQ.front().address == 0);
        REQUIRE(uut.PQ.front().event_cycle == old_event_cycle + hit_latency);
      }

      mock_ll._operate();

      AND_THEN("The packet is translated") {
        REQUIRE(uut.PQ.front().address == 0x11111eef);
        REQUIRE(uut.PQ.front().v_address == test.v_address);
        REQUIRE(uut.pq_has_ready());
      }
    }
  }
}

