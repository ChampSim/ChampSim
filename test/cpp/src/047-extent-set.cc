#include <catch.hpp>

#include "extent_set.h"

TEST_CASE("An extent set can be constructed with size 0") {
  champsim::extent_set uut{};
  REQUIRE(std::tuple_size<decltype(uut)>::value == 0);
}

TEST_CASE("An extent set can be constructed with size 1") {
  champsim::extent_set uut{champsim::dynamic_extent{20,16}};
  REQUIRE(std::tuple_size<decltype(uut)>::value == 1);
  REQUIRE(uut.size() == 1);
  REQUIRE(uut.get<0>().upper == 20);
  REQUIRE(uut.get<0>().lower == 16);
  REQUIRE(uut.bit_size() == 4);
}

TEST_CASE("An extent set can be constructed with a static slice") {
  champsim::extent_set uut{champsim::static_extent<20,16>{}};
  REQUIRE(std::tuple_size<decltype(uut)>::value == 1);
  REQUIRE(uut.size() == 1);
  REQUIRE(uut.get<0>().upper == 20);
  REQUIRE(uut.get<0>().lower == 16);
  REQUIRE(uut.bit_size() == 4);
}


TEST_CASE("An extent set can be constructed with size 2") {
  champsim::extent_set uut{champsim::dynamic_extent{20,16}, champsim::dynamic_extent{24,20}};
  REQUIRE(std::tuple_size<decltype(uut)>::value == 2);
  REQUIRE(uut.size() == 2);
  REQUIRE(uut.get<0>().upper == 20);
  REQUIRE(uut.get<0>().lower == 16);
  REQUIRE(uut.get<1>().upper == 24);
  REQUIRE(uut.get<1>().lower == 20);
  REQUIRE(uut.bit_size() == 8);
}

TEST_CASE("There is a free function to create contiguous slice sets") {
  champsim::extent_set uut{champsim::make_contiguous_extent_set(16,4,4)};
  REQUIRE(std::tuple_size<decltype(uut)>::value == 2);
  REQUIRE(uut.size() == 2);
  REQUIRE(uut.get<0>().upper == 20);
  REQUIRE(uut.get<0>().lower == 16);
  REQUIRE(uut.get<1>().upper == 24);
  REQUIRE(uut.get<1>().lower == 20);
  REQUIRE(uut.bit_size() == 8);
}

TEST_CASE("An extent set can partition an address") {
  champsim::extent_set uut{champsim::dynamic_extent{20,16}, champsim::dynamic_extent{24,20}};
  champsim::address addr{0x00ab0000};
  auto [low, up] = uut(addr);
  REQUIRE(low == champsim::address_slice{champsim::dynamic_extent{20,16}, 0xb});
  REQUIRE(up == champsim::address_slice{champsim::dynamic_extent{24,20}, 0xa});
}

