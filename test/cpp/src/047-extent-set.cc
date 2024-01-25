#include <catch.hpp>

#include "extent_set.h"

using namespace champsim::data::data_literals;

TEST_CASE("An extent set can be constructed with size 0") {
  champsim::extent_set uut{};
  REQUIRE(std::tuple_size<decltype(uut)>::value == 0);
}

TEST_CASE("An extent set can be constructed with size 1") {
  champsim::extent_set uut{champsim::dynamic_extent{20_b,16_b}};
  REQUIRE(std::tuple_size<decltype(uut)>::value == 1);
  REQUIRE(uut.size() == 1);
  REQUIRE(uut.get<0>().upper == 20_b);
  REQUIRE(uut.get<0>().lower == 16_b);
  REQUIRE(uut.bit_size() == 4);
}

TEST_CASE("An extent set can be constructed with a static slice") {
  champsim::extent_set uut{champsim::static_extent<20_b,16_b>{}};
  REQUIRE(std::tuple_size<decltype(uut)>::value == 1);
  REQUIRE(uut.size() == 1);
  REQUIRE(uut.get<0>().upper == 20_b);
  REQUIRE(uut.get<0>().lower == 16_b);
  REQUIRE(uut.bit_size() == 4);
}


TEST_CASE("An extent set can be constructed with size 2") {
  champsim::extent_set uut{champsim::dynamic_extent{20_b,16_b}, champsim::dynamic_extent{24_b,20_b}};
  REQUIRE(std::tuple_size<decltype(uut)>::value == 2);
  REQUIRE(uut.size() == 2);
  REQUIRE(uut.get<0>().upper == 20_b);
  REQUIRE(uut.get<0>().lower == 16_b);
  REQUIRE(uut.get<1>().upper == 24_b);
  REQUIRE(uut.get<1>().lower == 20_b);
  REQUIRE(uut.bit_size() == 8);
}

TEST_CASE("There is a free function to create contiguous slice sets") {
  champsim::extent_set uut{champsim::make_contiguous_extent_set(16,4,4)};
  REQUIRE(std::tuple_size<decltype(uut)>::value == 2);
  REQUIRE(uut.size() == 2);
  REQUIRE(uut.get<0>().upper == 20_b);
  REQUIRE(uut.get<0>().lower == 16_b);
  REQUIRE(uut.get<1>().upper == 24_b);
  REQUIRE(uut.get<1>().lower == 20_b);
  REQUIRE(uut.bit_size() == 8);
}

TEST_CASE("An extent set can partition an address") {
  champsim::extent_set uut{champsim::dynamic_extent{20_b,16_b}, champsim::dynamic_extent{24_b,20_b}};
  champsim::address addr{0x00ab0000};
  auto [low, up] = uut(addr);
  REQUIRE(low == champsim::address_slice{champsim::dynamic_extent{20_b,16_b}, 0xb});
  REQUIRE(up == champsim::address_slice{champsim::dynamic_extent{24_b,20_b}, 0xa});
}

