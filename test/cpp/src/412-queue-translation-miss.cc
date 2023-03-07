#include <catch.hpp>
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

SCENARIO("Cache queues detect translation misses in WQ") {
  GIVEN("A write queue with one item") {
    constexpr uint64_t hit_latency = 5;
    do_nothing_MRC mock_ll{2*hit_latency};
    CACHE::TranslatingQueues uut{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
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

      uut._operate();
      mock_ll._operate();

      THEN("The packet is issued for translation") {
        REQUIRE(mock_ll.packet_count() == 1);
        REQUIRE_FALSE(uut.wq_has_ready());
      }

      // Operate enough cycles to hit
      for (uint64_t i = 0; i < (hit_latency+1); ++i) {
        mock_ll._operate();
        uut._operate();
      }

      AND_THEN("A miss is detected") {
        REQUIRE_FALSE(uut.wq_has_ready());
      }

      // Operate long enough for the return to happen
      for (uint64_t i = 0; i < 100; ++i) {
        mock_ll._operate();
        uut._operate();
      }

      AND_THEN("The packet is translated") {
        REQUIRE(uut.WQ.front().v_address == test.v_address);
        REQUIRE(uut.WQ.front().event_cycle == old_event_cycle + 3*hit_latency);
        REQUIRE(uut.WQ.front().address == 0x11111eef);
        REQUIRE(uut.wq_has_ready());
      }
    }
  }
}

SCENARIO("Cache queues detect translation misses in RQ") {
  GIVEN("A read queue with one item") {
    constexpr uint64_t hit_latency = 5;
    do_nothing_MRC mock_ll{2*hit_latency};
    CACHE::TranslatingQueues uut{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
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

      uut._operate();
      mock_ll._operate();

      THEN("The packet is issued for translation") {
        REQUIRE(mock_ll.packet_count() == 1);
        REQUIRE_FALSE(uut.rq_has_ready());
      }

      // Operate enough cycles to hit
      for (uint64_t i = 0; i < (hit_latency+1); ++i) {
        mock_ll._operate();
        uut._operate();
      }

      AND_THEN("A miss is detected") {
        REQUIRE_FALSE(uut.rq_has_ready());
      }

      // Operate long enough for the return to happen
      for (uint64_t i = 0; i < 100; ++i) {
        mock_ll._operate();
        uut._operate();
      }

      AND_THEN("The packet is translated") {
        REQUIRE(uut.RQ.front().v_address == test.v_address);
        REQUIRE(uut.RQ.front().event_cycle == old_event_cycle + 3*hit_latency);
        REQUIRE(uut.RQ.front().address == 0x11111eef);
        REQUIRE(uut.rq_has_ready());
      }
    }
  }
}

SCENARIO("Cache queues detect translation misses in PQ") {
  GIVEN("A prefetch queue with one item") {
    constexpr uint64_t hit_latency = 5;
    do_nothing_MRC mock_ll{2*hit_latency};
    CACHE::TranslatingQueues uut{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
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

      uut._operate();
      mock_ll._operate();

      THEN("The packet is issued for translation") {
        REQUIRE(mock_ll.packet_count() == 1);
        REQUIRE_FALSE(uut.pq_has_ready());
      }

      // Operate enough cycles to hit
      for (uint64_t i = 0; i < (hit_latency+1); ++i) {
        mock_ll._operate();
        uut._operate();
      }

      AND_THEN("A miss is detected") {
        REQUIRE_FALSE(uut.pq_has_ready());
      }

      // Operate long enough for the return to happen
      for (uint64_t i = 0; i < 100; ++i) {
        mock_ll._operate();
        uut._operate();
      }

      AND_THEN("The packet is translated") {
        REQUIRE(uut.PQ.front().v_address == test.v_address);
        REQUIRE(uut.PQ.front().event_cycle == old_event_cycle + 3*hit_latency);
        REQUIRE(uut.PQ.front().address == 0x11111eef);
        REQUIRE(uut.pq_has_ready());
      }
    }
  }
}
