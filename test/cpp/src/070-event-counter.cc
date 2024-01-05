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

