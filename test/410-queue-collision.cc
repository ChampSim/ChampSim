#include "catch.hpp"
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

extern bool warmup_complete[NUM_CPUS];

template <typename Q>
void issue_wq(Q &uut, PACKET pkt)
{
  // Issue it to the uut
  auto result = uut.add_wq(pkt);
  REQUIRE(result);

  uut._operate();
}

template <typename Q>
void issue_rq(Q &uut, PACKET pkt)
{
  // Issue it to the uut
  auto result = uut.add_rq(pkt);
  REQUIRE(result);

  uut._operate();
}

template <typename Q>
void issue_pq(Q &uut, PACKET pkt)
{
  // Issue it to the uut
  auto result = uut.add_pq(pkt);
  REQUIRE(result);

  uut._operate();
}

template <typename Q, typename F>
void issue(Q &uut, uint64_t seed_addr, MemoryRequestProducer *ret, F func)
{
  // Create a test packet
  PACKET seed;
  seed.address = seed_addr;
  seed.v_address = 0;
  seed.cpu = 0;
  seed.to_return = {ret};

  std::invoke(func, uut, seed);
}

template <typename Q, typename F>
void issue(Q &uut, uint64_t seed_addr, F func)
{
  // Create a test packet
  PACKET seed;
  seed.address = seed_addr;
  seed.v_address = 0;
  seed.cpu = 0;

  std::invoke(func, uut, seed);
}

template <typename Q>
void wq_to_wq()
{
  GIVEN("A write queue with one item") {
    constexpr uint64_t address = 0xdeadbeef;
    Q uut{1, 32, 32, 32, 1, LOG2_BLOCK_SIZE, false};

    // Turn off warmup
    std::fill(std::begin(warmup_complete), std::end(warmup_complete), true);

    issue(uut, address, issue_wq<decltype(uut)>);

    WHEN("A packet with the same address is sent") {
      issue(uut, address, issue_wq<decltype(uut)>);

      THEN("The two packets are merged") {
        REQUIRE(std::size(uut.WQ) == 1);
      }
    }
  }
}

template <typename Q>
void rq_to_rq()
{
  GIVEN("A read queue with one item") {
    constexpr uint64_t address = 0xdeadbeef;
    Q uut{1, 32, 32, 32, 1, LOG2_BLOCK_SIZE, false};

    // These are just here to give us pointers to MemoryRequestProducers
    to_wq_MRP ul0{nullptr}, ul1{nullptr};

    // Turn off warmup
    std::fill(std::begin(warmup_complete), std::end(warmup_complete), true);

    issue(uut, address, &ul0, issue_rq<decltype(uut)>);

    WHEN("A packet with the same address is sent") {
      issue(uut, address, &ul1, issue_rq<decltype(uut)>);

      THEN("The two packets are merged") {
        REQUIRE(std::size(uut.RQ) == 1);
        REQUIRE(std::size(uut.RQ.front().to_return) == 2);
        REQUIRE(std::count(std::begin(uut.RQ.front().to_return), std::end(uut.RQ.front().to_return), &ul0) == 1);
        REQUIRE(std::count(std::begin(uut.RQ.front().to_return), std::end(uut.RQ.front().to_return), &ul1) == 1);
      }
    }
  }
}

template <typename Q>
void wq_to_rq()
{
  GIVEN("A write queue with one item") {
    constexpr uint64_t address = 0xdeadbeef;
    Q uut{1, 32, 32, 32, 1, LOG2_BLOCK_SIZE, false};

    // Turn off warmup
    std::fill(std::begin(warmup_complete), std::end(warmup_complete), true);

    issue(uut, address, issue_wq<decltype(uut)>);

    WHEN("A packet with the same address is sent to the read queue") {
      counting_MRP counter;
      issue(uut, address, &counter, issue_rq<decltype(uut)>);

      THEN("The two packets are merged") {
        REQUIRE(std::size(uut.WQ) == 1);
        REQUIRE(std::size(uut.RQ) == 0);
        REQUIRE(counter.count == 1);
      }
    }
  }
}

template <typename Q>
void pq_to_pq()
{
  GIVEN("A prefetch queue with one item") {
    constexpr uint64_t address = 0xdeadbeef;
    Q uut{1, 32, 32, 32, 1, LOG2_BLOCK_SIZE, false};

    // These are just here to give us pointers to MemoryRequestProducers
    to_wq_MRP ul0{nullptr}, ul1{nullptr};

    // Turn off warmup
    std::fill(std::begin(warmup_complete), std::end(warmup_complete), true);

    issue(uut, address, &ul0, issue_pq<decltype(uut)>);

    WHEN("A packet with the same address is sent") {
      issue(uut, address, &ul1, issue_pq<decltype(uut)>);

      THEN("The two packets are merged") {
        REQUIRE(std::size(uut.PQ) == 1);
        REQUIRE(std::size(uut.PQ.front().to_return) == 2);
        REQUIRE(std::count(std::begin(uut.PQ.front().to_return), std::end(uut.PQ.front().to_return), &ul0) == 1);
        REQUIRE(std::count(std::begin(uut.PQ.front().to_return), std::end(uut.PQ.front().to_return), &ul1) == 1);
      }
    }
  }
}

template <typename Q>
void wq_to_pq()
{
  GIVEN("A write queue with one item") {
    constexpr uint64_t address = 0xdeadbeef;
    Q uut{1, 32, 32, 32, 1, LOG2_BLOCK_SIZE, false};

    // Turn off warmup
    std::fill(std::begin(warmup_complete), std::end(warmup_complete), true);

    issue(uut, address, issue_wq<decltype(uut)>);

    WHEN("A packet with the same address is sent to the prefetch queue") {
      counting_MRP counter;
      issue(uut, address, &counter, issue_pq<decltype(uut)>);

      THEN("The two packets are merged") {
        REQUIRE(std::size(uut.WQ) == 1);
        REQUIRE(std::size(uut.PQ) == 0);
        REQUIRE(counter.count == 1);
      }
    }
  }
}

SCENARIO("Non-translating cache queues forward WQ to WQ") {
  wq_to_wq<CACHE::NonTranslatingQueues>();
}

SCENARIO("Translating cache queues forward WQ to WQ") {
  wq_to_wq<CACHE::TranslatingQueues>();
}

SCENARIO("Non-translating cache queues forward RQ to RQ") {
  rq_to_rq<CACHE::NonTranslatingQueues>();
}

SCENARIO("Translating cache queues forward RQ to RQ") {
  rq_to_rq<CACHE::TranslatingQueues>();
}

SCENARIO("Non-translating cache queues forward WQ to RQ") {
  wq_to_rq<CACHE::NonTranslatingQueues>();
}

SCENARIO("Translating cache queues forward WQ to RQ") {
  wq_to_rq<CACHE::TranslatingQueues>();
}

SCENARIO("Non-translating cache queues forward PQ to PQ") {
  pq_to_pq<CACHE::NonTranslatingQueues>();
}

SCENARIO("Translating cache queues forward PQ to PQ") {
  pq_to_pq<CACHE::TranslatingQueues>();
}

SCENARIO("Non-translating cache queues forward WQ to PQ") {
  wq_to_pq<CACHE::NonTranslatingQueues>();
}

SCENARIO("Translating cache queues forward WQ to PQ") {
  wq_to_pq<CACHE::TranslatingQueues>();
}
