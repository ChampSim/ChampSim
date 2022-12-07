#include "catch.hpp"
#include "mocks.hpp"

#include "cache.h"
#include "champsim_constants.h"

struct miss_testbed
{
  constexpr static uint64_t hit_latency = 5;
  constexpr static champsim::address address_that_will_hit{0xcafebabe};
  filter_MRC mock_ll{address_that_will_hit};
  CACHE::TranslatingQueues uut{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};

  miss_testbed()
  {
    uut.lower_level = &mock_ll;
  }

  virtual void issue(PACKET pkt) = 0;

  void setup()
  {
    // Turn off warmup
    uut.warmup = false;
    uut.begin_phase();

    // Create a test packet
    PACKET seed;
    seed.address = champsim::address{0xdeadbeef};
    seed.v_address = champsim::address{0xdeadbeef};
    seed.cpu = 0;

    issue(seed);

    // Operate enough cycles to realize we've missed
    for (uint64_t i = 0; i < (hit_latency+1); ++i) {
      operate();
    }
  }

  void operate()
  {
    mock_ll._operate();
    uut._operate();
  }
};

struct miss_wq_testbed : public miss_testbed
{
  using miss_testbed::miss_testbed;
  void issue(PACKET pkt) override
  {
    auto result = uut.add_wq(pkt);
    REQUIRE(result);
  }
};

struct miss_rq_testbed : public miss_testbed
{
  using miss_testbed::miss_testbed;
  void issue(PACKET pkt) override
  {
    auto result = uut.add_rq(pkt);
    REQUIRE(result);
  }
};

struct miss_pq_testbed : public miss_testbed
{
  using miss_testbed::miss_testbed;
  void issue(PACKET pkt) override
  {
    auto result = uut.add_pq(pkt);
    REQUIRE(result);
  }
};

SCENARIO("Translation misses in the WQ do not inhibit other translations from being issued") {
  GIVEN("A write queue with one item that has missed") {
    miss_wq_testbed testbed;
    testbed.setup();

    REQUIRE_FALSE(testbed.uut.wq_has_ready());

    WHEN("A packet is sent") {
      PACKET test;
      test.address = testbed.address_that_will_hit;
      test.v_address = testbed.address_that_will_hit;
      test.cpu = 0;

      testbed.issue(test);

      auto old_event_cycle = testbed.uut.current_cycle;

      // Operate long enough for the return to happen
      for (uint64_t i = 0; i < 100; ++i)
        testbed.operate();

      THEN("The packet is translated") {
        REQUIRE(std::size(testbed.uut.WQ) == 2);
        REQUIRE(testbed.uut.WQ.front().v_address == test.v_address);
        REQUIRE(testbed.uut.WQ.front().event_cycle == old_event_cycle + testbed.hit_latency);
        REQUIRE(testbed.uut.wq_has_ready());
      }
    }
  }
}

SCENARIO("Translation misses in the WQ do not inhibit packets that do not need translation") {
  GIVEN("A write queue with one item that has missed") {
    miss_wq_testbed testbed;
    testbed.setup();

    REQUIRE_FALSE(testbed.uut.wq_has_ready());

    WHEN("A translated packet is sent") {
      PACKET test;
      test.address = champsim::address{0xfeedcafe};
      test.v_address = champsim::address{0xdeadbeef};
      test.cpu = 0;

      testbed.issue(test);

      auto old_event_cycle = testbed.uut.current_cycle;

      // Operate long enough the hit to occur
      for (uint64_t i = 0; i < (testbed.hit_latency+1); ++i)
        testbed.operate();

      THEN("The packet is ready") {
        REQUIRE(std::size(testbed.uut.WQ) == 2);
        REQUIRE(testbed.uut.WQ.front().v_address == test.v_address);
        REQUIRE(testbed.uut.WQ.front().event_cycle == old_event_cycle + testbed.hit_latency);
        REQUIRE(testbed.uut.wq_has_ready());
      }
    }
  }
}


SCENARIO("Translation misses in the RQ do not inhibit other translations from being issued") {
  GIVEN("A read queue with one item that has missed") {
    miss_rq_testbed testbed;
    testbed.setup();

    REQUIRE_FALSE(testbed.uut.rq_has_ready());

    WHEN("A packet is sent") {
      PACKET test;
      test.address = testbed.address_that_will_hit;
      test.v_address = testbed.address_that_will_hit;
      test.cpu = 0;

      testbed.issue(test);

      auto old_event_cycle = testbed.uut.current_cycle;

      // Operate long enough for the return to happen
      for (uint64_t i = 0; i < 100; ++i)
        testbed.operate();

      THEN("The packet is translated") {
        REQUIRE(std::size(testbed.uut.RQ) == 2);
        REQUIRE(testbed.uut.RQ.front().v_address == test.v_address);
        REQUIRE(testbed.uut.RQ.front().event_cycle == old_event_cycle + testbed.hit_latency);
        REQUIRE(testbed.uut.rq_has_ready());
      }
    }
  }
}

SCENARIO("Translation misses in the RQ do not inhibit packets that do not need translation") {
  GIVEN("A read queue with one item that has missed") {
    miss_rq_testbed testbed;
    testbed.setup();

    REQUIRE_FALSE(testbed.uut.rq_has_ready());

    WHEN("A translated packet is sent") {
      PACKET test;
      test.address = champsim::address{0xfeedcafe};
      test.v_address = champsim::address{0xdeadbeef};
      test.cpu = 0;

      testbed.issue(test);

      auto old_event_cycle = testbed.uut.current_cycle;

      // Operate long enough the hit to occur
      for (uint64_t i = 0; i < (testbed.hit_latency+1); ++i)
        testbed.operate();

      THEN("The packet is ready") {
        REQUIRE(std::size(testbed.uut.RQ) == 2);
        REQUIRE(testbed.uut.RQ.front().v_address == test.v_address);
        REQUIRE(testbed.uut.RQ.front().event_cycle == old_event_cycle + testbed.hit_latency);
        REQUIRE(testbed.uut.rq_has_ready());
      }
    }
  }
}

SCENARIO("Translation misses in the PQ do not inhibit other translations from being issued") {
  GIVEN("A prefetch queue with one item that has missed") {
    miss_pq_testbed testbed;
    testbed.setup();

    REQUIRE_FALSE(testbed.uut.pq_has_ready());

    WHEN("A packet is sent") {
      PACKET test;
      test.address = testbed.address_that_will_hit;
      test.v_address = testbed.address_that_will_hit;
      test.cpu = 0;

      testbed.issue(test);

      auto old_event_cycle = testbed.uut.current_cycle;

      // Operate long enough for the return to happen
      for (uint64_t i = 0; i < 100; ++i)
        testbed.operate();

      THEN("The packet is translated") {
        REQUIRE(std::size(testbed.uut.PQ) == 2);
        REQUIRE(testbed.uut.PQ.front().v_address == test.v_address);
        REQUIRE(testbed.uut.PQ.front().event_cycle == old_event_cycle + testbed.hit_latency);
        REQUIRE(testbed.uut.pq_has_ready());
      }
    }
  }
}

SCENARIO("Translation misses in the PQ do not inhibit packets that do not need translation") {
  GIVEN("A prefetch queue with one item that has missed") {
    miss_pq_testbed testbed;
    testbed.setup();

    REQUIRE_FALSE(testbed.uut.pq_has_ready());

    WHEN("A translated packet is sent") {
      PACKET test;
      test.address = champsim::address{0xfeedcafe};
      test.v_address = champsim::address{0xdeadbeef};
      test.cpu = 0;

      testbed.issue(test);

      auto old_event_cycle = testbed.uut.current_cycle;

      // Operate long enough the hit to occur
      for (uint64_t i = 0; i < (testbed.hit_latency+1); ++i)
        testbed.operate();

      THEN("The packet is ready") {
        REQUIRE(std::size(testbed.uut.PQ) == 2);
        REQUIRE(testbed.uut.PQ.front().v_address == test.v_address);
        REQUIRE(testbed.uut.PQ.front().event_cycle == old_event_cycle + testbed.hit_latency);
        REQUIRE(testbed.uut.pq_has_ready());
      }
    }
  }
}

