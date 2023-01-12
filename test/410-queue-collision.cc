#include "catch.hpp"
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

template <typename Q>
bool issue_wq (Q& uut, PACKET pkt)
{
  // Issue it to the uut
  auto result = uut.add_wq(pkt);
  uut._operate();
  return result;
}

template <typename Q>
bool issue_rq (Q& uut, PACKET pkt)
{
  // Issue it to the uut
  auto result = uut.add_rq(pkt);
  uut._operate();
  return result;
}

template <typename Q>
bool issue_pq (Q& uut, PACKET pkt)
{
  // Issue it to the uut
  auto result = uut.add_pq(pkt);
  uut._operate();
  return result;
}

template <typename Q>
bool issue_pq_fill_this_level(Q &uut, PACKET pkt)
{
  // Issue it to the uut
  pkt.fill_this_level = true;
  auto result = uut.add_pq(pkt);
  uut._operate();
  return result;
}

template <typename Q, typename F>
bool issue(Q &uut, uint64_t seed_addr, uint16_t asid, MemoryRequestProducer *ret, F&& func)
{
  // Create a test packet
  PACKET seed;
  seed.address = seed_addr;
  seed.v_address = 0;
  seed.asid = asid;
  seed.cpu = 0;
  seed.to_return = {ret};

  return std::invoke(std::forward<F>(func), uut, seed);
}

template <typename Q, typename F>
bool issue(Q &uut, uint64_t seed_addr, uint16_t asid, F&& func)
{
  // Create a test packet
  PACKET seed;
  seed.address = seed_addr;
  seed.v_address = 0;
  seed.asid = asid;
  seed.cpu = 0;

  return std::invoke(std::forward<F>(func), uut, seed);
}

template <typename Q, typename F>
bool issue_non_translated(Q &uut, uint64_t seed_addr, uint16_t asid, MemoryRequestProducer *ret, F&& func)
{
  // Create a test packet
  PACKET seed;
  seed.address = seed_addr;
  seed.v_address = seed_addr;
  seed.asid = asid;
  seed.cpu = 0;
  seed.to_return = {ret};

  return std::invoke(std::forward<F>(func), uut, seed);
}

TEMPLATE_TEST_CASE("Cache queues perform forwarding WQ to WQ", "", CACHE::NonTranslatingQueues, CACHE::TranslatingQueues) {
  GIVEN("An empty write queue") {
    constexpr uint64_t address = 0xdeadbeef;
    constexpr uint16_t asid = 10;
    TestType uut{1, 32, 32, 32, 0, 1, LOG2_BLOCK_SIZE, false};

    // Turn off warmup
    uut.warmup = false;
    uut.begin_phase();

    THEN("The statistics are zero") {
      CHECK(uut.sim_stats.back().WQ_ACCESS == 0);
      CHECK(uut.sim_stats.back().WQ_TO_CACHE == 0);
      CHECK(uut.sim_stats.back().WQ_MERGED == 0);
    }

    WHEN("A packet is sent to the write queue") {
      auto seed_result = issue(uut, address, asid, issue_wq<TestType>);
      THEN("The issue is accepted") {
        REQUIRE(seed_result);

        AND_THEN("The statistics reflect the issue") {
          CHECK(uut.sim_stats.back().WQ_ACCESS == 1);
          CHECK(uut.sim_stats.back().WQ_TO_CACHE == 1);
        }
      }

      AND_WHEN("A packet with the same address is sent to the write queue") {
        auto test_result = issue(uut, address, asid, issue_wq<TestType>);
        THEN("The issue is accepted") {
          REQUIRE(test_result);

          AND_THEN("The statistics reflect the issue") {
            CHECK(uut.sim_stats.back().WQ_ACCESS == 2);
            CHECK(uut.sim_stats.back().WQ_TO_CACHE == 2);
          }
        }

        THEN("The two packets are merged") {
          REQUIRE(std::size(uut.WQ) == 1);

          AND_THEN("The statistics reflect the merge") {
            REQUIRE(uut.sim_stats.back().WQ_MERGED == 1);
          }
        }
      }

      AND_WHEN("A packet with the same address but a different asid is sent") {
        issue(uut, address, asid+1, issue_wq<TestType>);

        THEN("The two packets are not merged") {
          REQUIRE(std::size(uut.WQ) == 2);
        }
      }
    }
  }
}

TEMPLATE_TEST_CASE("Cache queues perform forwarding RQ to RQ", "", CACHE::NonTranslatingQueues, CACHE::TranslatingQueues) {
  GIVEN("An empty write queue") {
    constexpr uint64_t address = 0xdeadbeef;
    constexpr uint16_t asid = 10;
    TestType uut{1, 32, 32, 32, 0, 1, LOG2_BLOCK_SIZE, false};

    // These are here to give us pointers to MRPs
    to_rq_MRP<CACHE> ul0{nullptr}, ul1{nullptr};

    // Turn off warmup
    uut.warmup = false;
    uut.begin_phase();

    THEN("The statistics are zero") {
      CHECK(uut.sim_stats.back().RQ_ACCESS == 0);
      CHECK(uut.sim_stats.back().RQ_TO_CACHE == 0);
      CHECK(uut.sim_stats.back().RQ_MERGED == 0);
    }

    WHEN("A packet is sent to the read queue") {
      auto seed_result = issue(uut, address, asid, &ul0, issue_rq<TestType>);
      THEN("The issue is accepted") {
        REQUIRE(seed_result);

        AND_THEN("The statistics reflect the issue") {
          CHECK(uut.sim_stats.back().RQ_ACCESS == 1);
          CHECK(uut.sim_stats.back().RQ_TO_CACHE == 1);
        }
      }

      AND_WHEN("A packet with the same address is sent to the read queue") {
        auto test_result = issue(uut, address, asid, &ul1, issue_rq<TestType>);
        THEN("The issue is accepted") {
          REQUIRE(test_result);

          AND_THEN("The statistics reflect the issue") {
            CHECK(uut.sim_stats.back().RQ_ACCESS == 2);
            CHECK(uut.sim_stats.back().RQ_TO_CACHE == 2);
          }
        }

        THEN("The two packets are merged") {
          CHECK(std::size(uut.RQ) == 1);
          CHECK(std::size(uut.RQ.front().to_return) == 2);
          CHECK(std::count(std::begin(uut.RQ.front().to_return), std::end(uut.RQ.front().to_return), &ul0) == 1);
          CHECK(std::count(std::begin(uut.RQ.front().to_return), std::end(uut.RQ.front().to_return), &ul1) == 1);

          AND_THEN("The statistics reflect the merge") {
            REQUIRE(uut.sim_stats.back().RQ_MERGED == 1);
          }
        }
      }

      AND_WHEN("A packet with the same address but different asid is sent") {
        issue(uut, address, asid+1, &ul1, issue_rq<TestType>);

        THEN("The two packets are not merged") {
          REQUIRE(std::size(uut.RQ) == 2);
          REQUIRE(std::size(uut.RQ.front().to_return) == 1);
          REQUIRE(std::count(std::begin(uut.RQ.front().to_return), std::end(uut.RQ.front().to_return), &ul0) == 1);
          REQUIRE(std::count(std::begin(uut.RQ.back().to_return), std::end(uut.RQ.back().to_return), &ul1) == 1);
        }
      }
    }
  }
}

TEMPLATE_TEST_CASE("Cache queues perform forwarding PQ to PQ", "", CACHE::NonTranslatingQueues, CACHE::TranslatingQueues) {
  GIVEN("An empty prefetch queue") {
    constexpr uint64_t address = 0xdeadbeef;
    constexpr uint16_t asid = 10;
    TestType uut{1, 32, 32, 32, 0, 1, LOG2_BLOCK_SIZE, false};

    // These are here to give us pointers to MRPs
    to_rq_MRP<CACHE> ul0{nullptr}, ul1{nullptr};

    // Turn off warmup
    uut.warmup = false;
    uut.begin_phase();

    THEN("The statistics are zero") {
      CHECK(uut.sim_stats.back().PQ_ACCESS == 0);
      CHECK(uut.sim_stats.back().PQ_TO_CACHE == 0);
      CHECK(uut.sim_stats.back().PQ_MERGED == 0);
    }

    WHEN("A packet is sent to the prefetch queue") {
      auto seed_result = issue(uut, address, asid, &ul0, issue_pq<TestType>);
      THEN("The issue is accepted") {
        REQUIRE(seed_result);

        AND_THEN("The statistics reflect the issue") {
          CHECK(uut.sim_stats.back().PQ_ACCESS == 1);
          CHECK(uut.sim_stats.back().PQ_TO_CACHE == 1);
        }
      }

      AND_WHEN("A packet with the same address is sent to the prefetch queue") {
        auto test_result = issue(uut, address, asid, &ul1, issue_pq<TestType>);
        THEN("The issue is accepted") {
          REQUIRE(test_result);

          AND_THEN("The statistics reflect the issue") {
            CHECK(uut.sim_stats.back().PQ_ACCESS == 2);
            CHECK(uut.sim_stats.back().PQ_TO_CACHE == 2);
          }
        }

        THEN("The two packets are merged") {
          CHECK(std::size(uut.PQ) == 1);
          CHECK(std::size(uut.PQ.front().to_return) == 2);
          CHECK(std::count(std::begin(uut.PQ.front().to_return), std::end(uut.PQ.front().to_return), &ul0) == 1);
          CHECK(std::count(std::begin(uut.PQ.front().to_return), std::end(uut.PQ.front().to_return), &ul1) == 1);

          AND_THEN("The statistics reflect the merge") {
            REQUIRE(uut.sim_stats.back().PQ_MERGED == 1);
          }
        }
      }

      AND_WHEN("A packet with the same address but different asid is sent to the read queue") {
        issue(uut, address, asid+1, &ul1, issue_pq<decltype(uut)>);

        THEN("The two packets are not merged") {
          REQUIRE(std::size(uut.PQ) == 2);
          CHECK(std::size(uut.PQ.front().to_return) == 1);
          CHECK(std::count(std::begin(uut.PQ.at(0).to_return), std::end(uut.PQ.at(0).to_return), &ul0) == 1);
          CHECK(std::count(std::begin(uut.PQ.at(1).to_return), std::end(uut.PQ.at(1).to_return), &ul1) == 1);
        }
      }
    }
  }
}

TEMPLATE_TEST_CASE("Cache queues forward WQ to RQ", "", CACHE::NonTranslatingQueues, CACHE::TranslatingQueues) {
  GIVEN("An empty write queue and read queue") {
    constexpr uint64_t address = 0xdeadbeef;
    constexpr uint16_t asid = 10;
    TestType uut{1, 32, 32, 32, 0, 1, LOG2_BLOCK_SIZE, false};

    // Turn off warmup
    uut.warmup = false;
    uut.begin_phase();

    WHEN("A packet is sent to the write queue") {
      auto seed_result = issue(uut, address, asid, issue_wq<TestType>);
      THEN("The issue is accepted") {
        REQUIRE(seed_result);

        AND_THEN("The statistics reflect the issue") {
          CHECK(uut.sim_stats.back().WQ_ACCESS == 1);
          CHECK(uut.sim_stats.back().WQ_TO_CACHE == 1);
        }
      }

      AND_WHEN("A packet with the same address is sent to the read queue") {
        counting_MRP counter;
        issue(uut, address, asid, &counter, issue_rq<TestType>);

        THEN("The two packets are merged") {
          CHECK(std::size(uut.WQ) == 1);
          CHECK(std::size(uut.RQ) == 0);
          CHECK(counter.count == 1);

          AND_THEN("The statistics reflect the merge") {
            REQUIRE(uut.sim_stats.back().WQ_FORWARD == 1);
          }
        }
      }

      AND_WHEN("A packet with the same address but different asid is sent") {
        counting_MRP counter;
        issue(uut, address, asid+1, &counter, issue_rq<TestType>);

        THEN("The two packets are not merged") {
          CHECK(std::size(uut.WQ) == 1);
          CHECK(std::size(uut.RQ) == 1);
          CHECK(counter.count == 0);
        }
      }
    }
  }
}

TEMPLATE_TEST_CASE("Cache queues forward WQ to PQ", "", CACHE::NonTranslatingQueues, CACHE::TranslatingQueues) {
  GIVEN("An empty write queue and prefetch queue") {
    constexpr uint64_t address = 0xdeadbeef;
    constexpr uint16_t asid = 10;
    TestType uut{1, 32, 32, 32, 0, 1, LOG2_BLOCK_SIZE, false};

    // Turn off warmup
    uut.warmup = false;
    uut.begin_phase();

    WHEN("A packet is sent to the write queue") {
      auto seed_result = issue(uut, address, asid, issue_wq<TestType>);
      THEN("The issue is accepted") {
        REQUIRE(seed_result);

        AND_THEN("The statistics reflect the issue") {
          CHECK(uut.sim_stats.back().WQ_ACCESS == 1);
          CHECK(uut.sim_stats.back().WQ_TO_CACHE == 1);
        }
      }

      WHEN("A packet with the same address is sent to the prefetch queue") {
        counting_MRP counter;
        issue(uut, address, asid, &counter, issue_pq<TestType>);

        THEN("The two packets are merged") {
          CHECK(std::size(uut.WQ) == 1);
          CHECK(std::size(uut.PQ) == 0);
          CHECK(counter.count == 1);

          AND_THEN("The statistics reflect the merge") {
            REQUIRE(uut.sim_stats.back().WQ_FORWARD == 1);
          }
        }
      }

      AND_WHEN("A packet with the same address but different asid is sent to the prefetch queue") {
        counting_MRP counter;
        issue(uut, address, asid+1, &counter, issue_pq<TestType>);

        THEN("The two packets are not merged") {
          REQUIRE(std::size(uut.WQ) == 1);
          REQUIRE(std::size(uut.PQ) == 1);
          REQUIRE(counter.count == 0);
        }
      }
    }
  }
}

SCENARIO("Translating cache queues forward RQ virtual to physical RQ") {
  GIVEN("A read queue with one item") {
    constexpr uint64_t address = 0xdeadbeef;
    constexpr uint16_t asid = 10;
    do_nothing_MRC mock_ll{2};
    CACHE::TranslatingQueues uut{1, 32, 32, 32, 0, 1, LOG2_BLOCK_SIZE, false};
    uut.lower_level = &mock_ll;

    // These are just here to give us pointers to MemoryRequestProducers
    to_wq_MRP<CACHE> ul0{nullptr}, ul1{nullptr};

    // Turn off warmup
    uut.warmup = false;
    uut.begin_phase();

    issue(uut, address, asid, &ul0, issue_rq<decltype(uut)>);

    WHEN("A packet with the same physical address but non translated is sent") {
      issue_non_translated(uut, address, asid, &ul1, issue_rq<decltype(uut)>);

      THEN("The two packets are not merged") {
        REQUIRE(std::size(uut.RQ) == 2);
      }
    }
  }
}

SCENARIO("Non-translating cache queues forward PQ to PQ with different fill levels") {
  GIVEN("A prefetch queue with one item") {
    constexpr uint64_t address = 0xdeadbeef;
    constexpr uint16_t asid = 10;
    CACHE::NonTranslatingQueues uut{1, 32, 32, 32, 0, 1, LOG2_BLOCK_SIZE, false};

    // These are just here to give us pointers to MemoryRequestProducers
    to_wq_MRP<CACHE> ul0{nullptr}, ul1{nullptr};

    // Turn off warmup
    uut.warmup = false;
    uut.begin_phase();

    issue(uut, address, asid, &ul0, issue_pq<decltype(uut)>);

    WHEN("A packet with the same address but different fill level is sent") {
      issue(uut, address, asid, &ul1, issue_pq_fill_this_level<decltype(uut)>);

      THEN("The two packets are merged and fill this level") {
        REQUIRE(std::size(uut.PQ) == 1);
        REQUIRE(uut.PQ.front().fill_this_level == true);
      }
    }
  }
}
