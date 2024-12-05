#include <catch.hpp>

#include "event_counter.h"

TEST_CASE("An event counter can allocate") {
  champsim::stats::event_counter<int> uut{};
  constexpr typename decltype(uut)::key_type key = 2016;
  uut.allocate(key);
  REQUIRE(uut.at(key) == 0);
}

TEST_CASE("An event counter can increment") {
  champsim::stats::event_counter<int> uut{};
  constexpr typename decltype(uut)::key_type key = 2016;
  uut.increment(key);
  REQUIRE(uut.at(key) == 1);
  uut.increment(key);
  REQUIRE(uut.at(key) == 2);
}

TEST_CASE("An event counter can set the value") {
  champsim::stats::event_counter<int> uut{};
  constexpr typename decltype(uut)::key_type key = 2016;
  constexpr typename decltype(uut)::value_type value = 100;
  uut.set(key, value);
  REQUIRE(uut.at(key) == value);
}

TEST_CASE("An event counter can give a substitue value in the case of missing data") {
  champsim::stats::event_counter<int> uut{};
  constexpr typename decltype(uut)::key_type key = 2016;
  REQUIRE(uut.value_or(key, 3) == 3);
}

TEST_CASE("An event counter can deallocate after allocation") {
  champsim::stats::event_counter<int> uut{};
  constexpr typename decltype(uut)::key_type key = 2016;
  constexpr typename decltype(uut)::value_type value = 100;
  uut.set(key, value);
  uut.deallocate(key);
  REQUIRE(uut.value_or(key, 3) == 3);
}

TEST_CASE("Two event counters can be added") {
  champsim::stats::event_counter<int> lhs{};
  champsim::stats::event_counter<int> rhs{};
  constexpr typename decltype(lhs)::key_type key = 2016;
  constexpr typename decltype(lhs)::value_type lhs_value = 100;
  constexpr typename decltype(lhs)::value_type rhs_value = 20;
  lhs.set(key, lhs_value);
  rhs.set(key, rhs_value);
  REQUIRE((lhs + rhs).at(key) == lhs_value + rhs_value);
}

TEST_CASE("Two event counters can be subtracted") {
  champsim::stats::event_counter<int> lhs{};
  champsim::stats::event_counter<int> rhs{};
  constexpr typename decltype(lhs)::key_type key = 2016;
  constexpr typename decltype(lhs)::value_type lhs_value = 100;
  constexpr typename decltype(lhs)::value_type rhs_value = 20;
  lhs.set(key, lhs_value);
  rhs.set(key, rhs_value);
  REQUIRE((lhs - rhs).at(key) == lhs_value - rhs_value);
}

