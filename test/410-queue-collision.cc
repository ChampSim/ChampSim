#include "catch.hpp"
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

extern bool warmup_complete[NUM_CPUS];

SCENARIO("Cache queues forward WQ to WQ") {
  GIVEN("A write queue with one item") {
    CACHE::NonTranslatingQueues uut{1, 32, 32, 32, 1, LOG2_BLOCK_SIZE, false};

    // Turn off warmup
    std::fill(std::begin(warmup_complete), std::end(warmup_complete), true);

    // Create a test packet
    PACKET seed;
    seed.address = 0xdeadbeef;
    seed.cpu = 0;

    // Issue it to the uut
    auto seed_result = uut.add_wq(seed);
    REQUIRE(seed_result);

    WHEN("A packet with the same address is sent") {
      auto test_result = uut.add_wq(seed);
      REQUIRE(test_result);

      uut._operate();

      THEN("The two packets are merged") {
        REQUIRE(std::size(uut.WQ) == 1);
      }
    }
  }
}

SCENARIO("Cache queues forward RQ to RQ") {
  GIVEN("A read queue with one item") {
    CACHE::NonTranslatingQueues uut{1, 32, 32, 32, 1, LOG2_BLOCK_SIZE, false};

    // These are just here to give us pointers to MemoryRequestProducers
    to_wq_MRP ul0{nullptr}, ul1{nullptr};

    // Turn off warmup
    std::fill(std::begin(warmup_complete), std::end(warmup_complete), true);

    // Create a test packet
    PACKET seed;
    seed.address = 0xdeadbeef;
    seed.cpu = 0;
    seed.to_return = {&ul0};

    // Issue it to the uut
    auto seed_result = uut.add_rq(seed);
    REQUIRE(seed_result);

    WHEN("A packet with the same address is sent") {
      auto test = seed;
      test.to_return = {&ul1};
      auto test_result = uut.add_rq(test);
      REQUIRE(test_result);

      uut._operate();

      THEN("The two packets are merged") {
        REQUIRE(std::size(uut.RQ) == 1);
        REQUIRE(std::size(uut.RQ.front().to_return) == 2);
        REQUIRE(std::count(std::begin(uut.RQ.front().to_return), std::end(uut.RQ.front().to_return), &ul0) == 1);
        REQUIRE(std::count(std::begin(uut.RQ.front().to_return), std::end(uut.RQ.front().to_return), &ul1) == 1);
      }
    }
  }
}

SCENARIO("Cache queues forward WQ to RQ") {
  GIVEN("A write queue with one item") {
    CACHE::NonTranslatingQueues uut{1, 32, 32, 32, 1, LOG2_BLOCK_SIZE, false};

    counting_MRP counter;

    // Turn off warmup
    std::fill(std::begin(warmup_complete), std::end(warmup_complete), true);

    // Create a test packet
    PACKET seed;
    seed.address = 0xdeadbeef;
    seed.cpu = 0;

    // Issue it to the uut
    auto seed_result = uut.add_wq(seed);
    REQUIRE(seed_result);

    WHEN("A packet with the same address is sent to the read queue") {
      auto test = seed;
      test.to_return = {&counter};
      auto test_result = uut.add_rq(test);
      REQUIRE(test_result);

      uut._operate();

      THEN("The two packets are merged") {
        REQUIRE(std::size(uut.WQ) == 1);
        REQUIRE(std::size(uut.RQ) == 0);
        REQUIRE(counter.count == 1);
      }
    }
  }
}

SCENARIO("Cache queues forward PQ to PQ") {
  GIVEN("A prefetch queue with one item") {
    CACHE::NonTranslatingQueues uut{1, 32, 32, 32, 1, LOG2_BLOCK_SIZE, false};

    // These are just here to give us pointers to MemoryRequestProducers
    to_wq_MRP ul0{nullptr}, ul1{nullptr};

    // Turn off warmup
    std::fill(std::begin(warmup_complete), std::end(warmup_complete), true);

    // Create a test packet
    PACKET seed;
    seed.address = 0xdeadbeef;
    seed.cpu = 0;
    seed.to_return = {&ul0};

    // Issue it to the uut
    auto seed_result = uut.add_pq(seed);
    REQUIRE(seed_result);

    WHEN("A packet with the same address is sent") {
      auto test = seed;
      test.to_return = {&ul1};
      auto test_result = uut.add_pq(test);
      REQUIRE(test_result);

      uut._operate();

      THEN("The two packets are merged") {
        REQUIRE(std::size(uut.PQ) == 1);
        REQUIRE(std::size(uut.PQ.front().to_return) == 2);
        REQUIRE(std::count(std::begin(uut.PQ.front().to_return), std::end(uut.PQ.front().to_return), &ul0) == 1);
        REQUIRE(std::count(std::begin(uut.PQ.front().to_return), std::end(uut.PQ.front().to_return), &ul1) == 1);
      }
    }
  }
}

SCENARIO("Cache queues forward WQ to PQ") {
  GIVEN("A write queue with one item") {
    CACHE::NonTranslatingQueues uut{1, 32, 32, 32, 1, LOG2_BLOCK_SIZE, false};

    counting_MRP counter;

    // Turn off warmup
    std::fill(std::begin(warmup_complete), std::end(warmup_complete), true);

    // Create a test packet
    PACKET seed;
    seed.address = 0xdeadbeef;
    seed.cpu = 0;

    // Issue it to the uut
    auto seed_result = uut.add_wq(seed);
    REQUIRE(seed_result);

    WHEN("A packet with the same address is sent to the prefetch queue") {
      auto test = seed;
      test.to_return = {&counter};
      auto test_result = uut.add_pq(test);
      REQUIRE(test_result);

      uut._operate();

      THEN("The two packets are merged") {
        REQUIRE(std::size(uut.WQ) == 1);
        REQUIRE(std::size(uut.PQ) == 0);
        REQUIRE(counter.count == 1);
      }
    }
  }
}
