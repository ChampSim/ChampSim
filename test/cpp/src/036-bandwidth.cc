#include <catch.hpp>

#include "util/detect.h"
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
  champsim::bandwidth::maximum_type maxval{10};
  champsim::bandwidth test_a{maxval};
  champsim::bandwidth test_b{maxval};

  test_a.consume();
  test_b.consume(1);

  REQUIRE(test_a.amount_consumed() == test_b.amount_consumed());
  REQUIRE(test_a.amount_remaining() == test_b.amount_remaining());
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

template <typename T>
using can_increment = decltype( std::declval<T>()++ );

template <typename T>
using can_add = decltype( std::declval<T>()+1 );

template <typename T>
using can_decrement = decltype( std::declval<T>()-- );

template <typename T>
using can_subtract = decltype( std::declval<T>()-1 );

TEST_CASE("Bandwidth maximums are not incrementable")
{
  STATIC_REQUIRE_FALSE(champsim::is_detected_v<can_increment, champsim::bandwidth::maximum_type>);
  STATIC_REQUIRE_FALSE(champsim::is_detected_v<can_add, champsim::bandwidth::maximum_type>);
  STATIC_REQUIRE_FALSE(champsim::is_detected_v<can_decrement, champsim::bandwidth::maximum_type>);
  STATIC_REQUIRE_FALSE(champsim::is_detected_v<can_subtract, champsim::bandwidth::maximum_type>);
}
