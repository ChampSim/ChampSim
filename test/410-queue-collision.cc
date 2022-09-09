#include "catch.hpp"
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

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
void issue(Q &uut, uint64_t seed_addr, uint16_t asid, MemoryRequestProducer *ret, F func)
{
  // Create a test packet
  PACKET seed;
  seed.address = seed_addr;
  seed.v_address = 0;
  seed.asid = asid;
  seed.cpu = 0;
  seed.to_return = {ret};

  std::invoke(func, uut, seed);
}

template <typename Q, typename F>
void issue(Q &uut, uint64_t seed_addr, uint16_t asid, F func)
{
  // Create a test packet
  PACKET seed;
  seed.address = seed_addr;
  seed.v_address = 0;
  seed.asid = asid;
  seed.cpu = 0;

  std::invoke(func, uut, seed);
}

template <typename Q>
void wq_to_wq()
{
  GIVEN("A write queue with one item") {
    constexpr uint64_t address = 0xdeadbeef;
    constexpr uint16_t asid = 10;
    Q uut{1, 32, 32, 32, 1, LOG2_BLOCK_SIZE, false};

    // Turn off warmup
    uut.warmup = false;
    uut.begin_phase();

    issue(uut, address, asid, issue_wq<decltype(uut)>);

    WHEN("A packet with the same address and asid is sent") {
      issue(uut, address, asid, issue_wq<decltype(uut)>);

      THEN("The two packets are merged") {
        REQUIRE(std::size(uut.WQ) == 1);
      }
    }

    WHEN("A packet with the same address but a different asid is sent") {
      issue(uut, address, asid+1, issue_wq<decltype(uut)>);

      THEN("The two packets are not merged") {
        REQUIRE(std::size(uut.WQ) == 2);
      }
    }
  }
}

template <typename Q>
void rq_to_rq()
{
  GIVEN("A read queue with one item") {
    constexpr uint64_t address = 0xdeadbeef;
    constexpr uint16_t asid = 10;
    Q uut{1, 32, 32, 32, 1, LOG2_BLOCK_SIZE, false};

    // These are just here to give us pointers to MemoryRequestProducers
    to_wq_MRP ul0{nullptr}, ul1{nullptr};

    // Turn off warmup
    uut.warmup = false;
    uut.begin_phase();

    issue(uut, address, asid, &ul0, issue_rq<decltype(uut)>);

    WHEN("A packet with the same address and asid is sent") {
      issue(uut, address, asid, &ul1, issue_rq<decltype(uut)>);

      THEN("The two packets are merged") {
        REQUIRE(std::size(uut.RQ) == 1);
        REQUIRE(std::size(uut.RQ.front().to_return) == 2);
        REQUIRE(std::count(std::begin(uut.RQ.front().to_return), std::end(uut.RQ.front().to_return), &ul0) == 1);
        REQUIRE(std::count(std::begin(uut.RQ.front().to_return), std::end(uut.RQ.front().to_return), &ul1) == 1);
      }
    }

    WHEN("A packet with the same address but different asid is sent") {
      issue(uut, address, asid+1, &ul1, issue_rq<decltype(uut)>);

      THEN("The two packets are not merged") {
        REQUIRE(std::size(uut.RQ) == 2);
        REQUIRE(std::size(uut.RQ.front().to_return) == 1);
        REQUIRE(std::count(std::begin(uut.RQ.front().to_return), std::end(uut.RQ.front().to_return), &ul0) == 1);
        REQUIRE(std::count(std::begin(uut.RQ.back().to_return), std::end(uut.RQ.back().to_return), &ul1) == 1);
      }
    }
  }
}

template <typename Q>
void wq_to_rq()
{
  GIVEN("A write queue with one item") {
    constexpr uint64_t address = 0xdeadbeef;
    constexpr uint16_t asid = 10;
    Q uut{1, 32, 32, 32, 1, LOG2_BLOCK_SIZE, false};

    // Turn off warmup
    uut.warmup = false;
    uut.begin_phase();

    issue(uut, address, asid, issue_wq<decltype(uut)>);

    WHEN("A packet with the same address and asid is sent to the read queue") {
      counting_MRP counter;
      issue(uut, address, asid, &counter, issue_rq<decltype(uut)>);

      THEN("The two packets are merged") {
        REQUIRE(std::size(uut.WQ) == 1);
        REQUIRE(std::size(uut.RQ) == 0);
        REQUIRE(counter.count == 1);
      }
    }

    WHEN("A packet with the same address but different asid is sent to the read queue") {
      counting_MRP counter;
      issue(uut, address, asid+1, &counter, issue_rq<decltype(uut)>);

      THEN("The two packets are not merged") {
        REQUIRE(std::size(uut.WQ) == 1);
        REQUIRE(std::size(uut.RQ) == 1);
        REQUIRE(counter.count == 0);
      }
    }
  }
}

template <typename Q>
void pq_to_pq()
{
  GIVEN("A prefetch queue with one item") {
    constexpr uint64_t address = 0xdeadbeef;
    constexpr uint16_t asid = 10;
    Q uut{1, 32, 32, 32, 1, LOG2_BLOCK_SIZE, false};

    // These are just here to give us pointers to MemoryRequestProducers
    to_wq_MRP ul0{nullptr}, ul1{nullptr};

    // Turn off warmup
    uut.warmup = false;
    uut.begin_phase();

    issue(uut, address, asid, &ul0, issue_pq<decltype(uut)>);

    WHEN("A packet with the same address and asid is sent") {
      issue(uut, address, asid, &ul1, issue_pq<decltype(uut)>);

      THEN("The two packets are merged") {
        REQUIRE(std::size(uut.PQ) == 1);
        REQUIRE(std::size(uut.PQ.front().to_return) == 2);
        REQUIRE(std::count(std::begin(uut.PQ.front().to_return), std::end(uut.PQ.front().to_return), &ul0) == 1);
        REQUIRE(std::count(std::begin(uut.PQ.front().to_return), std::end(uut.PQ.front().to_return), &ul1) == 1);
      }
    }

    WHEN("A packet with the same address but different asid is sent") {
      issue(uut, address, asid+1, &ul1, issue_pq<decltype(uut)>);

      THEN("The two packets are not merged") {
        REQUIRE(std::size(uut.PQ) == 2);
        REQUIRE(std::size(uut.PQ.front().to_return) == 1);
        REQUIRE(std::count(std::begin(uut.PQ.front().to_return), std::end(uut.PQ.front().to_return), &ul0) == 1);
        REQUIRE(std::count(std::begin(uut.PQ.back().to_return), std::end(uut.PQ.back().to_return), &ul1) == 1);
      }
    }
  }
}

template <typename Q>
void wq_to_pq()
{
  GIVEN("A write queue with one item") {
    constexpr uint64_t address = 0xdeadbeef;
    constexpr uint16_t asid = 10;
    Q uut{1, 32, 32, 32, 1, LOG2_BLOCK_SIZE, false};

    // Turn off warmup
    uut.warmup = false;
    uut.begin_phase();

    issue(uut, address, asid, issue_wq<decltype(uut)>);

    WHEN("A packet with the same address and asid is sent to the prefetch queue") {
      counting_MRP counter;
      issue(uut, address, asid, &counter, issue_pq<decltype(uut)>);

      THEN("The two packets are merged") {
        REQUIRE(std::size(uut.WQ) == 1);
        REQUIRE(std::size(uut.PQ) == 0);
        REQUIRE(counter.count == 1);
      }
    }

    WHEN("A packet with the same address but different asid is sent to the prefetch queue") {
      counting_MRP counter;
      issue(uut, address, asid+1, &counter, issue_pq<decltype(uut)>);

      THEN("The two packets are not merged") {
        REQUIRE(std::size(uut.WQ) == 1);
        REQUIRE(std::size(uut.PQ) == 1);
        REQUIRE(counter.count == 0);
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
