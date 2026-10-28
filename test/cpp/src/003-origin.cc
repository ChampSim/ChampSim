#include <catch.hpp>

#include "origin.h"

TEST_CASE("An origin exposes the same identities under canonical and domain names")
{
  champsim::origin uut{3, 7};

  // The aliases are the same identity: cpu() IS the consumer, asid() IS the producer
  REQUIRE(uut.consumer() == 3);
  REQUIRE(uut.cpu() == uut.consumer());
  REQUIRE(uut.producer() == 7);
  REQUIRE(uut.asid() == uut.producer());
}

TEST_CASE("A default origin is invalid in both coordinates")
{
  champsim::origin uut{};
  REQUIRE_FALSE(uut.has_consumer());
  REQUIRE_FALSE(uut.has_producer());
  REQUIRE(uut.consumer() == champsim::origin::invalid_id);
  REQUIRE(uut.producer() == champsim::origin::invalid_id);
}

TEST_CASE("Origin derivation helpers replace one coordinate and keep the other")
{
  champsim::origin base{1, 2};

  auto moved = base.with_consumer(9); // e.g. a producer migrating between consumers
  REQUIRE(moved.consumer() == 9);
  REQUIRE(moved.producer() == 2);

  auto respaced = base.with_producer(5); // e.g. a trace record overriding the producer id
  REQUIRE(respaced.consumer() == 1);
  REQUIRE(respaced.producer() == 5);
}

TEST_CASE("Origins compare by both coordinates")
{
  REQUIRE(champsim::origin{1, 2} == champsim::origin{1, 2});
  REQUIRE(champsim::origin{1, 2} != champsim::origin{1, 3});
  REQUIRE(champsim::origin{1, 2} != champsim::origin{2, 2});
  REQUIRE(champsim::origin{1, 2} < champsim::origin{1, 3});
  REQUIRE(champsim::origin{1, 3} < champsim::origin{2, 0});
}
