#include "catch.hpp"
#include "mocks.hpp"
#include "channel.h"
#include "champsim_constants.h"

SCENARIO("Cache queues issue translations in WQ") {
  GIVEN("A write queue with one item") {
    constexpr uint64_t hit_latency = 1;
    do_nothing_MRC mock_ll;
    champsim::TranslatingQueues uut{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    uut.lower_level = &mock_ll;

    // Turn off warmup
    uut.warmup = false;
    uut.begin_phase();

    WHEN("A packet is sent") {
      // Create a test packet
      PACKET test;
      test.address = 0xdeadbeef;
      test.v_address = 0xdeadbeef;
      test.is_translated = false;
      test.cpu = 0;

      auto test_result = uut.add_wq(test);
      THEN("The issue is accepted") {
        REQUIRE(test_result);
      }

      auto old_event_cycle = uut.current_cycle;

      mock_ll._operate();
      uut._operate();

      THEN("The packet is issued for translation") {
        REQUIRE(mock_ll.packet_count() == 1);
        REQUIRE(uut.WQ.front().address == 0);
        REQUIRE(uut.WQ.front().event_cycle == old_event_cycle + hit_latency);
      }

      mock_ll._operate();
      uut._operate();

      AND_THEN("The packet is translated") {
        REQUIRE(uut.WQ.front().address == 0x11111eef);
        REQUIRE(uut.WQ.front().v_address == test.v_address);
        REQUIRE(uut.WQ.front().is_translated);
      }
    }
  }
}


SCENARIO("Cache queues issue translations in RQ") {
  GIVEN("A read queue with one item") {
    constexpr uint64_t hit_latency = 1;
    do_nothing_MRC mock_ll;
    champsim::TranslatingQueues uut{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    uut.lower_level = &mock_ll;

    // Turn off warmup
    uut.warmup = false;
    uut.begin_phase();

    WHEN("A packet is sent") {
      // Create a test packet
      PACKET test;
      test.address = 0xdeadbeef;
      test.v_address = 0xdeadbeef;
      test.is_translated = false;
      test.cpu = 0;

      auto test_result = uut.add_rq(test);
      THEN("The issue is accepted") {
        REQUIRE(test_result);
      }

      auto old_event_cycle = uut.current_cycle;

      mock_ll._operate();
      uut._operate();

      THEN("The packet is issued for translation") {
        REQUIRE(mock_ll.packet_count() == 1);
        REQUIRE(uut.RQ.front().address == 0);
        REQUIRE(uut.RQ.front().event_cycle == old_event_cycle + hit_latency);
      }

      mock_ll._operate();
      uut._operate();

      AND_THEN("The packet is translated") {
        REQUIRE(uut.RQ.front().address == 0x11111eef);
        REQUIRE(uut.RQ.front().v_address == test.v_address);
        REQUIRE(uut.RQ.front().is_translated);
      }
    }
  }
}


SCENARIO("Cache queues issue translations in PQ") {
  GIVEN("A prefetch queue with one item") {
    constexpr uint64_t hit_latency = 1;
    do_nothing_MRC mock_ll;
    champsim::TranslatingQueues uut{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    uut.lower_level = &mock_ll;

    // Turn off warmup
    uut.warmup = false;
    uut.begin_phase();

    WHEN("A packet is sent") {
      // Create a test packet
      PACKET test;
      test.address = 0xdeadbeef;
      test.v_address = 0xdeadbeef;
      test.is_translated = false;
      test.cpu = 0;

      auto test_result = uut.add_pq(test);
      THEN("The issue is accepted") {
        REQUIRE(test_result);
      }

      auto old_event_cycle = uut.current_cycle;

      mock_ll._operate();
      uut._operate();

      THEN("The packet is issued for translation") {
        REQUIRE(mock_ll.packet_count() == 1);
        REQUIRE(uut.PQ.front().address == 0);
        REQUIRE(uut.PQ.front().event_cycle == old_event_cycle + hit_latency);
      }

      mock_ll._operate();
      uut._operate();

      AND_THEN("The packet is translated") {
        REQUIRE(uut.PQ.front().address == 0x11111eef);
        REQUIRE(uut.PQ.front().v_address == test.v_address);
        REQUIRE(uut.PQ.front().is_translated);
      }
    }
  }
}

SCENARIO("Translations in the WQ work even if the addresses happen to be the same") {
  GIVEN("A write queue with one item") {
    constexpr uint64_t hit_latency = 1;
    release_MRC mock_ll;
    champsim::TranslatingQueues uut{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    uut.lower_level = &mock_ll;

    // Turn off warmup
    uut.warmup = false;
    uut.begin_phase();

    WHEN("A packet is sent") {
      // Create a test packet
      PACKET test;
      test.address = 0xdeadbeef;
      test.v_address = test.address;
      test.is_translated = false;
      test.data = test.address; // smuggle our own translation through the mock
      test.cpu = 0;

      auto test_result = uut.add_wq(test);
      THEN("The issue is accepted") {
        REQUIRE(test_result);
      }

      auto old_event_cycle = uut.current_cycle;

      mock_ll._operate();
      uut._operate();

      THEN("The packet is issued for translation") {
        REQUIRE(mock_ll.packet_count() == 1);
        REQUIRE(uut.WQ.front().address == 0);
        REQUIRE(uut.WQ.front().event_cycle == old_event_cycle + hit_latency);
      }

      mock_ll.release(test.address);
      uut._operate();

      AND_THEN("The packet is translated") {
        REQUIRE(uut.WQ.front().address == uut.WQ.front().v_address);
        REQUIRE(uut.WQ.front().v_address == test.v_address);
        REQUIRE(uut.WQ.front().is_translated);
      }
    }
  }
}


SCENARIO("Translations in the RQ work even if the addresses happen to be the same") {
  GIVEN("A read queue with one item") {
    constexpr uint64_t hit_latency = 1;
    release_MRC mock_ll;
    champsim::TranslatingQueues uut{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    uut.lower_level = &mock_ll;

    // Turn off warmup
    uut.warmup = false;
    uut.begin_phase();

    WHEN("A packet is sent") {
      // Create a test packet
      PACKET test;
      test.address = 0xdeadbeef;
      test.v_address = test.address;
      test.is_translated = false;
      test.data = test.address;
      test.cpu = 0;

      auto test_result = uut.add_rq(test);
      THEN("The issue is accepted") {
        REQUIRE(test_result);
      }

      auto old_event_cycle = uut.current_cycle;

      mock_ll._operate();
      uut._operate();

      THEN("The packet is issued for translation") {
        REQUIRE(mock_ll.packet_count() == 1);
        REQUIRE(uut.RQ.front().address == 0);
        REQUIRE(uut.RQ.front().event_cycle == old_event_cycle + hit_latency);
      }

      mock_ll.release(test.address);
      uut._operate();

      AND_THEN("The packet is translated") {
        REQUIRE(uut.RQ.front().address == uut.RQ.front().v_address);
        REQUIRE(uut.RQ.front().v_address == test.v_address);
        REQUIRE(uut.RQ.front().is_translated);
      }
    }
  }
}


SCENARIO("Translations in the PQ work even if the addresses happen to be the same") {
  GIVEN("A prefetch queue with one item") {
    constexpr uint64_t hit_latency = 1;
    release_MRC mock_ll;
    champsim::TranslatingQueues uut{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    uut.lower_level = &mock_ll;

    // Turn off warmup
    uut.warmup = false;
    uut.begin_phase();

    WHEN("A packet is sent") {
      // Create a test packet
      PACKET test;
      test.address = 0xdeadbeef;
      test.v_address = test.address;
      test.is_translated = false;
      test.data = test.address;
      test.cpu = 0;

      auto test_result = uut.add_pq(test);
      THEN("The issue is accepted") {
        REQUIRE(test_result);
      }

      auto old_event_cycle = uut.current_cycle;

      mock_ll._operate();
      uut._operate();

      THEN("The packet is issued for translation") {
        REQUIRE(mock_ll.packet_count() == 1);
        REQUIRE(uut.PQ.front().address == 0);
        REQUIRE(uut.PQ.front().event_cycle == old_event_cycle + hit_latency);
      }

      mock_ll.release(test.address);
      uut._operate();

      AND_THEN("The packet is translated") {
        REQUIRE(uut.PQ.front().address == uut.PQ.front().address);
        REQUIRE(uut.PQ.front().v_address == test.v_address);
        REQUIRE(uut.PQ.front().is_translated);
      }
    }
  }
}


