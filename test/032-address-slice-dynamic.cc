#include "catch.hpp"

#include "address.h"

#include <type_traits>

TEST_CASE("A dynamically-sized address slice is constructible by certian means") {
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent>, champsim::address>);
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent>, std::size_t, std::size_t, champsim::address>);
  STATIC_REQUIRE(std::is_copy_constructible_v<champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent>>);
  STATIC_REQUIRE(std::is_move_constructible_v<champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent>>);
  STATIC_REQUIRE(std::is_copy_assignable_v<champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent>>);
  STATIC_REQUIRE(std::is_move_assignable_v<champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent>>);
  STATIC_REQUIRE(std::is_destructible_v<champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent>>);
  STATIC_REQUIRE(std::is_swappable_v<champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent>>);
  STATIC_REQUIRE(std::is_assignable_v<champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent>, champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent>>);
}

TEST_CASE("Dynamically-sized address slices can be compared if their begins and ends are the same") {
  champsim::address addr_a{0xdeadbeef};
  champsim::address addr_b{0xcafefeed};

  REQUIRE(champsim::address_slice{16,8,addr_a} == champsim::address_slice{16,8,addr_a});
  REQUIRE_FALSE(champsim::address_slice{16,8,addr_a} != champsim::address_slice{16,8,addr_a});
  REQUIRE(champsim::address_slice{16,8,addr_a} != champsim::address_slice{16,8,addr_b});
  REQUIRE_FALSE(champsim::address_slice{16,8,addr_a} == champsim::address_slice{16,8,addr_b});
}

TEST_CASE("Dynamically-sized address slices can compare equal even if the underlying addresses differ outside the slice") {
  champsim::address addr_a{0xcafefeef};
  champsim::address addr_b{0xcafefeed};

  REQUIRE(champsim::address_slice{16,8,addr_a} == champsim::address_slice{16,8,addr_b});
  REQUIRE_FALSE(champsim::address_slice{16,8,addr_a} != champsim::address_slice{16,8,addr_b});
  REQUIRE(champsim::address_slice{20,12,addr_a} == champsim::address_slice{20,12,addr_b});
  REQUIRE_FALSE(champsim::address_slice{20,12,addr_a} != champsim::address_slice{20,12,addr_b});
  REQUIRE(champsim::address_slice{24,16,addr_a} == champsim::address_slice{24,16,addr_b});
  REQUIRE_FALSE(champsim::address_slice{24,16,addr_a} != champsim::address_slice{24,16,addr_b});
  REQUIRE(champsim::address_slice{28,20,addr_a} == champsim::address_slice{28,20,addr_b});
  REQUIRE_FALSE(champsim::address_slice{28,20,addr_a} != champsim::address_slice{28,20,addr_b});
  REQUIRE(champsim::address_slice{2,0,addr_a} != champsim::address_slice{2,0,addr_b});
  REQUIRE_FALSE(champsim::address_slice{2,0,addr_a} == champsim::address_slice{2,0,addr_b});
}

TEST_CASE("Dynamically-sized address sliced can be re-sliced") {
  champsim::address addr{0xabcdef89};

  REQUIRE(champsim::address_slice{20,8,addr}.slice(8,0) == champsim::address_slice{16,8,addr});
}

