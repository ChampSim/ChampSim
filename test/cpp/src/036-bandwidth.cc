#include <catch.hpp>

#include "bandwidth.h"

TEST_CASE("The bandwidth initially is not consumed")
{
  champsim::bandwidth test_val{champsim::bandwidth::maximum_type{10}};

  REQUIRE(test_val.has_remaining());
  REQUIRE(test_val.amount_consumed() == 0);
  REQUIRE(test_val.amount_remaining() == 10);
}

TEST_CASE("Calling bandwidth::consume() without arguments is like calling it with '1'")
{
  champsim::bandwidth test_val{champsim::bandwidth::maximum_type{10}};

  test_val.consume();

  REQUIRE(test_val.amount_consumed() == 1);
  REQUIRE(test_val.amount_remaining() == 9);
}

TEST_CASE("Consuming bandwidth shows in the amount consumed")
{
  champsim::bandwidth test_val{champsim::bandwidth::maximum_type{10}};

  test_val.consume(3);

  REQUIRE(test_val.amount_consumed() == 3);
  REQUIRE(test_val.amount_remaining() == 7);
}

TEST_CASE("The bandwidth can be reset")
{
  champsim::bandwidth test_val{champsim::bandwidth::maximum_type{10}};

  test_val.consume(3);

  CHECK(test_val.amount_consumed() == 3);
  CHECK(test_val.amount_remaining() == 7);

  test_val.reset();

  REQUIRE(test_val.amount_consumed() == 0);
  REQUIRE(test_val.amount_remaining() == 10);
}

TEST_CASE("Consuming exactly the bandwidth shows nothing remaining")
{
  champsim::bandwidth test_val{champsim::bandwidth::maximum_type{10}};

  test_val.consume(10);

  REQUIRE_FALSE(test_val.has_remaining());
  REQUIRE(test_val.amount_consumed() == 10);
  REQUIRE(test_val.amount_remaining() == 0);
}

TEST_CASE("Consuming more than the bandwidth throws an exception")
{
  champsim::bandwidth test_val{champsim::bandwidth::maximum_type{10}};

  REQUIRE_THROWS(test_val.consume(11));
}

