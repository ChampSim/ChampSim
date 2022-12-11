#include "catch.hpp"

#include "champsim.h"
#include "address.h"

#include <type_traits>

TEST_CASE("A dynamic address slice is constructible from a uint64_t") {
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent>, uint64_t>);

  auto address = GENERATE(as<uint64_t>{}, 0xdeadbeef, 0xffff'ffff'ffff'ffff);

  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> test_a{address};
  REQUIRE(test_a.to<uint64_t>() == address);
}

TEST_CASE("A dynamic address slice takes constructor arguments that affect slice positioning") {
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent>, std::size_t, std::size_t, uint64_t>);

  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> test_a{4,0,0xf};
  REQUIRE(champsim::address{test_a} == champsim::address{0xf});

  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> test_b{12,4,0xee};
  REQUIRE(champsim::address{test_b} == champsim::address{0xee0});

  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> test_c{24,12,0xadb};
  REQUIRE(champsim::address{test_c} == champsim::address{0xadb000});
}

TEST_CASE("A dynamic address slice takes constructor arguments that affect slicing") {
  champsim::address address{0xdeadbeef};
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent>, std::size_t, std::size_t, champsim::address>);

  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> test_a{4,0,address};
  REQUIRE(test_a.to<uint64_t>() == 0xf);

  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> test_b{12,4,address};
  REQUIRE(test_b.to<uint64_t>() == 0xee);

  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> test_c{24,12,address};
  REQUIRE(test_c.to<uint64_t>() == 0xadb);
}

TEST_CASE("A dynamic address_slice is copy constructible") {
  STATIC_REQUIRE(std::is_copy_constructible_v<champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent>>);

  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> control{0xdeadbeef};
  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> test_a{0xdeadbeef};
  REQUIRE(test_a == control);

  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> test_b{test_a};
  REQUIRE(test_a == test_b);
}

TEST_CASE("A dynamic address_slice is move constructible") {
  STATIC_REQUIRE(std::is_move_constructible_v<champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent>>);

  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> control{0xdeadbeef};
  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> test_a{0xdeadbeef};
  REQUIRE(test_a == control);

  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> test_b{std::move(test_a)};
  REQUIRE(test_b == control);
}

TEST_CASE("A dynamic address_slice is copy assignable") {
  STATIC_REQUIRE(std::is_copy_assignable_v<champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent>>);

  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> test_a{};
  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> test_b{0xdeadbeef};
  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> control{0xdeadbeef};
  REQUIRE(test_b == control);
  REQUIRE(test_a != test_b);

  test_a = test_b;
  CHECK(test_a == test_b);
  CHECK(test_a == control);
  CHECK(test_b == control);
}

TEST_CASE("A dynamic address_slice is move assignable") {
  STATIC_REQUIRE(std::is_move_assignable_v<champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent>>);

  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> control{0xdeadbeef};
  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> test_a{0xdeadbeef};
  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> test_b{};
  REQUIRE(test_a == control);
  REQUIRE(test_b != control);

  test_b = std::move(test_a);
  REQUIRE(test_b == control);
}

TEST_CASE("A dynamic address_slice is swappable") {
  STATIC_REQUIRE(std::is_swappable_v<champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent>>);

  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> control_a{0xdeadbeef};
  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> test_a{0xdeadbeef};
  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> control_b{0xcafebabe};
  champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> test_b{0xcafebabe};

  REQUIRE(test_a == control_a);
  REQUIRE(test_b == control_b);

  std::swap(test_a, test_b);

  REQUIRE(test_b == control_a);
  REQUIRE(test_a == control_b);
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

  REQUIRE(champsim::address_slice{20,8,addr}.slice(8,2) == champsim::address_slice{16,10,addr});
}

TEST_CASE("A dynamically-sized address slice compares for equality") {
  champsim::address_slice lhs{20,16,10};
  REQUIRE_FALSE(lhs == champsim::address_slice{20,16,9});
  REQUIRE(lhs == champsim::address_slice{20,16,10});
  REQUIRE_FALSE(lhs == champsim::address_slice{20,16,11});
}

TEST_CASE("A dynamically-sized address slice compares for inequality") {
  champsim::address_slice lhs{20,16,10};
  REQUIRE(lhs != champsim::address_slice{20,16,9});
  REQUIRE_FALSE(lhs != champsim::address_slice{20,16,10});
  REQUIRE(lhs != champsim::address_slice{20,16,11});
}

TEST_CASE("A dynamically-sized address slice compares less") {
  champsim::address_slice lhs{20,16,10};
  REQUIRE_FALSE(lhs < champsim::address_slice{20,16,9});
  REQUIRE_FALSE(lhs < champsim::address_slice{20,16,10});
  REQUIRE(lhs < champsim::address_slice{20,16,11});
}

TEST_CASE("A dynamically-sized address slice compares less than or equal") {
  champsim::address_slice lhs{20,16,10};
  REQUIRE_FALSE(lhs <= champsim::address_slice{20,16,9});
  REQUIRE(lhs <= champsim::address_slice{20,16,10});
  REQUIRE(lhs <= champsim::address_slice{20,16,11});
}

TEST_CASE("A dynamically-sized address slice compares greater") {
  champsim::address_slice lhs{20,16,10};
  REQUIRE_FALSE(lhs < champsim::address_slice{20,16,9});
  REQUIRE_FALSE(lhs < champsim::address_slice{20,16,10});
  REQUIRE(lhs < champsim::address_slice{20,16,11});
}

TEST_CASE("A dynamically-sized address slice compares greater than or equal") {
  champsim::address_slice lhs{20,16,10};
  REQUIRE(lhs >= champsim::address_slice{20,16,9});
  REQUIRE(lhs >= champsim::address_slice{20,16,10});
  REQUIRE_FALSE(lhs >= champsim::address_slice{20,16,11});
}

TEST_CASE("A dynamically-sized address slice can add") {
  champsim::address_slice lhs{20,16,1};
  auto result = lhs + 1;
  REQUIRE(result == champsim::address_slice{20,16,2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("A dynamically-sized address slice can add in place") {
  champsim::address_slice lhs{20,16,1};
  lhs += 1;
  REQUIRE(lhs == champsim::address_slice{20,16,2});
  REQUIRE(champsim::address{lhs} == champsim::address{0x20000});
}

TEST_CASE("A dynamically-sized address slice can subtract") {
  champsim::address_slice lhs{20,16,3};
  auto result = lhs - 1;
  REQUIRE(result == champsim::address_slice{20,16,2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("A dynamically-sized address slice can subtract in place") {
  champsim::address_slice lhs{20,16,3};
  lhs -= 1;
  REQUIRE(lhs == champsim::address_slice{20,16,2});
  REQUIRE(champsim::address{lhs} == champsim::address{0x20000});
}

TEST_CASE("A dynamically-sized address slice can add a negative number") {
  champsim::address_slice lhs{20,16,3};
  auto result = lhs + -1;
  REQUIRE(result == champsim::address_slice{20,16,2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("A dynamically-sized address slice can add a negative number in place") {
  champsim::address_slice lhs{20,16,3};
  lhs += -1;
  REQUIRE(lhs == champsim::address_slice{20,16,2});
  REQUIRE(champsim::address{lhs} == champsim::address{0x20000});
}

TEST_CASE("A dynamically-sized address slice can subtract a negative number") {
  champsim::address_slice lhs{20,16,1};
  auto result = lhs - (-1);
  REQUIRE(result == champsim::address_slice{20,16,2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("A dynamically-sized address slice can subtract a negative number in place") {
  champsim::address_slice lhs{20,16,1};
  lhs -= -1;
  REQUIRE(lhs == champsim::address_slice{20,16,2});
  REQUIRE(champsim::address{lhs} == champsim::address{0x20000});
}

TEST_CASE("A const dynamically-sized address slice can add") {
  const champsim::address_slice lhs{20,16,1};
  auto result = lhs + 1;
  REQUIRE(result == champsim::address_slice{20,16,2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("A const dynamically-sized address slice can subtract") {
  const champsim::address_slice lhs{20,16,3};
  auto result = lhs - 1;
  REQUIRE(result == champsim::address_slice{20,16,2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

