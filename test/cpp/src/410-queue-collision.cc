#include <catch.hpp>

#include "channel.h"
#include "mocks.hpp"

template <typename Q>
bool issue_wq(Q& uut, typename Q::request_type pkt)
{
  return uut.add_wq(pkt);
}

template <typename Q>
bool issue_rq(Q& uut, typename Q::request_type pkt)
{
  return uut.add_rq(pkt);
}

template <typename Q>
bool issue_pq(Q& uut, typename Q::request_type pkt)
{
  return uut.add_pq(pkt);
}

template <typename Q, typename F>
bool issue(Q& uut, champsim::address seed_addr, bool want_return, F&& func)
{
  // Create a test packet
  typename Q::request_type seed;
  seed.address = seed_addr;
  seed.v_address = champsim::address{};
  seed.is_translated = true;
  seed.cpu = 0;
  seed.response_requested = want_return;

  return std::invoke(std::forward<F>(func), uut, seed);
}

template <typename Q, typename F>
bool issue(Q& uut, champsim::address seed_addr, F&& func)
{
  // Create a test packet
  typename Q::request_type seed;
  seed.address = seed_addr;
  seed.v_address = champsim::address{};
  seed.is_translated = true;
  seed.cpu = 0;

  return std::invoke(std::forward<F>(func), uut, seed);
}

template <typename Q, typename F>
bool issue_non_translated(Q& uut, champsim::address seed_addr, F&& func)
{
  // Create a test packet
  typename Q::request_type seed;
  seed.address = seed_addr;
  seed.v_address = seed_addr;
  seed.is_translated = false;
  seed.cpu = 0;

  return std::invoke(std::forward<F>(func), uut, seed);
}

SCENARIO("Cache queues forward WQ to RQ")
{
  GIVEN("An empty write queue and read queue")
  {
    champsim::address address{0xdeadbeef};
    champsim::channel uut{32, 32, 32, champsim::data::bits{LOG2_BLOCK_SIZE}, false};

    WHEN("A packet is sent to the write queue")
    {
      auto seed_result = issue(uut, address, issue_wq<champsim::channel>);
      THEN("The issue is accepted")
      {
        REQUIRE(seed_result);

        AND_THEN("The statistics reflect the issue")
        {
          CHECK(uut.sim_stats.WQ_ACCESS == 1);
          CHECK(uut.sim_stats.WQ_TO_CACHE == 1);
        }
      }

      AND_WHEN("A packet with the same address is sent to the read queue")
      {
        issue(uut, address, true, issue_rq<champsim::channel>);
        uut.check_collision();

        THEN("The two packets are merged")
        {
          CHECK(uut.wq_occupancy() == 1);
          CHECK(uut.rq_occupancy() == 0);
          CHECK(std::size(uut.returned) == 1);

          AND_THEN("The statistics reflect the merge") { REQUIRE(uut.sim_stats.WQ_FORWARD == 1); }
        }
      }
    }
  }
}

SCENARIO("Cache queues forward WQ to PQ")
{
  GIVEN("An empty write queue and prefetch queue")
  {
    champsim::address address{0xdeadbeef};
    champsim::channel uut{32, 32, 32, champsim::data::bits{LOG2_BLOCK_SIZE}, false};

    WHEN("A packet is sent to the write queue")
    {
      auto seed_result = issue(uut, address, issue_wq<champsim::channel>);
      THEN("The issue is accepted")
      {
        REQUIRE(seed_result);

        AND_THEN("The statistics reflect the issue")
        {
          CHECK(uut.sim_stats.WQ_ACCESS == 1);
          CHECK(uut.sim_stats.WQ_TO_CACHE == 1);
        }
      }

      WHEN("A packet with the same address is sent to the prefetch queue")
      {
        issue(uut, address, true, issue_pq<champsim::channel>);
        uut.check_collision();

        THEN("The two packets are merged")
        {
          CHECK(uut.wq_occupancy() == 1);
          CHECK(uut.pq_occupancy() == 0);
          CHECK(std::size(uut.returned) == 1);

          AND_THEN("The statistics reflect the merge") { REQUIRE(uut.sim_stats.WQ_FORWARD == 1); }
        }
      }
    }
  }
}

SCENARIO("Translating cache queues forward RQ virtual to physical RQ")
{
  GIVEN("A read queue with one item")
  {
    champsim::address address{0xdeadbeef};
    do_nothing_MRC mock_ll{2};
    champsim::channel uut{32, 32, 32, champsim::data::bits{LOG2_BLOCK_SIZE}, false};

    issue(uut, address, issue_rq<decltype(uut)>);

    WHEN("A packet with the same physical address but non translated is sent")
    {
      issue_non_translated(uut, address, issue_rq<decltype(uut)>);

      uut.check_collision();
      THEN("The two packets are not merged") { REQUIRE(uut.rq_occupancy() == 2); }
    }
  }
}
