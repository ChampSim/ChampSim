#include "catch.hpp"

#include "champsim.h"
#include "address.h"
#include "util/detect.h"

TEST_CASE("An address slice is constructible from a uint64_t") {
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<20,16>, uint64_t>);
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<64,16>, uint64_t>);

  auto address = GENERATE(as<uint64_t>{}, 0xdeadbeef, 0xffff'ffff'ffff'ffff);

  champsim::address_slice<20,16> test_a{address};
  REQUIRE(test_a.to<uint64_t>() == (address & champsim::bitmask(20-16)));

  champsim::address_slice<64,16> test_b{address};
  REQUIRE(test_b.to<uint64_t>() == (address & champsim::bitmask(64-16)));
}

TEMPLATE_TEST_CASE_SIG("An address slice is constexpr constructible from a uint64_t", "", ((uint64_t ADDR), ADDR), 0xdeadbeef, 0xffff'ffff'ffff'ffff) {
  constexpr champsim::address_slice<20,16> test_a{ADDR};
  STATIC_REQUIRE(test_a.to<uint64_t>() == (ADDR & champsim::bitmask(20-16)));

  constexpr champsim::address_slice<64,16> test_b{ADDR};
  STATIC_REQUIRE(test_b.to<uint64_t>() == (ADDR & champsim::bitmask(64-16)));
}

TEST_CASE("An address slice is constructible from a wider address_slice") {
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<20,16>, champsim::address_slice<24,12>>);

  auto addr = GENERATE(champsim::address{0x1234'5678'90ab'cdef}, champsim::address{0xabababab}, champsim::address{0xffff'ffff'ffff'ffff});

  champsim::address_slice<20,16> control{addr};
  champsim::address_slice<24,12> wide{addr};
  champsim::address_slice<20,16> narrow{wide};
  REQUIRE(narrow == control);
}

TEMPLATE_TEST_CASE_SIG("An address slice is constexpr constructible from a wider address_slice", "", ((uint64_t ADDR), ADDR), 0x1234'5678'90ab'cdef, 0xabababab, 0xffff'ffff'ffff'ffff) {
  constexpr champsim::address_slice<20,16> control{(ADDR & champsim::bitmask(20,16)) >> 16};
  constexpr champsim::address_slice<24,12> wide{(ADDR & champsim::bitmask(24,12)) >> 12};
  constexpr champsim::address_slice<20,16> narrow{wide};
  REQUIRE(narrow == control);
}

TEST_CASE("An address slice is constructible from an address_slice that shares a lower endpoint") {
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<20,16>, champsim::address_slice<24,16>>);

  auto addr = GENERATE(champsim::address{0x1234'5678'90ab'cdef}, champsim::address{0xabababab}, champsim::address{0xffff'ffff'ffff'ffff});

  champsim::address_slice<20,16> control{addr};
  champsim::address_slice<24,16> wide{addr};
  champsim::address_slice<20,16> narrow{wide};
  REQUIRE(narrow == control);
}

TEST_CASE("An address slice is constructible from an address_slice that shares an upper endpoint") {
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<20,16>, champsim::address_slice<20,12>>);

  auto addr = GENERATE(champsim::address{0x1234'5678'90ab'cdef}, champsim::address{0xabababab}, champsim::address{0xffff'ffff'ffff'ffff});

  champsim::address_slice<20,16> control{addr};
  champsim::address_slice<20,12> wide{addr};
  champsim::address_slice<20,16> narrow{wide};
  REQUIRE(narrow == control);
}

TEST_CASE("An address slice is constructible from a narrower address slice") {
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<24,12>, champsim::address_slice<20,16>>);

  auto addr = GENERATE(champsim::address{0x1234'5678'90ab'cdef}, champsim::address{0xabababab}, champsim::address{0xffff'ffff'ffff'ffff});

  champsim::address_slice<24,12> control{(addr.to<uint64_t>() & champsim::bitmask(20,16)) >> 12};
  champsim::address_slice<20,16> narrow{addr};
  champsim::address_slice<24,12> wide{narrow};
  REQUIRE(wide == control);
}

TEST_CASE("A static address slice is constructible from a dynamic address slice") {
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<20,12>, champsim::address_slice<champsim::dynamic_extent>>);

  champsim::address_slice<champsim::dynamic_extent> source{20,12,0xabc};
  champsim::address_slice<20,12> test_a{source};
  REQUIRE(test_a.to<uint64_t>() == source.to<uint64_t>());
  REQUIRE(test_a.upper_extent() == source.upper_extent());
  REQUIRE(test_a.lower_extent() == source.lower_extent());
}

TEST_CASE("An address_slice is copy constructible") {
  STATIC_REQUIRE(std::is_copy_constructible_v<champsim::address_slice<20,16>>);

  champsim::address_slice<20,16> control{0xffff'ffff'ffff'ffff};
  champsim::address_slice<20,16> test_a{0xffff'ffff'ffff'ffff};
  REQUIRE(test_a == control);

  champsim::address_slice<20,16> test_b{test_a};
  REQUIRE(test_a == test_b);
}

TEST_CASE("An address_slice is move constructible") {
  STATIC_REQUIRE(std::is_move_constructible_v<champsim::address_slice<20,16>>);

  champsim::address_slice<20,16> control{0xffff'ffff'ffff'ffff};
  champsim::address_slice<20,16> test_a{0xffff'ffff'ffff'ffff};
  REQUIRE(test_a == control);

  champsim::address_slice<20,16> test_b{std::move(test_a)};
  REQUIRE(test_b == control);
}

TEST_CASE("An address_slice is copy assignable") {
  STATIC_REQUIRE(std::is_copy_assignable_v<champsim::address_slice<20,16>>);

  champsim::address_slice<20,16> test_a{};
  champsim::address_slice<20,16> test_b{0xffff'ffff'ffff'ffff};
  champsim::address_slice<20,16> control{0xffff'ffff'ffff'ffff};
  REQUIRE(test_b == control);
  REQUIRE(test_a != test_b);

  test_a = test_b;
  REQUIRE(test_a == test_b);
  REQUIRE(test_a == control);
  REQUIRE(test_b == control);
}

TEST_CASE("An address_slice is move assignable") {
  STATIC_REQUIRE(std::is_move_assignable_v<champsim::address_slice<20,16>>);

  champsim::address_slice<20,16> control{0xffff'ffff'ffff'ffff};
  champsim::address_slice<20,16> test_a{0xffff'ffff'ffff'ffff};
  champsim::address_slice<20,16> test_b{};
  REQUIRE(test_a == control);
  REQUIRE(test_b != control);

  test_b = std::move(test_a);
  REQUIRE(test_b == control);
}

TEST_CASE("An address_slice is swappable") {
  STATIC_REQUIRE(std::is_swappable_v<champsim::address_slice<20,16>>);

  champsim::address_slice<20,16> control_a{0xffff'ffff'ffff'ffff};
  champsim::address_slice<20,16> test_a{0xffff'ffff'ffff'ffff};
  champsim::address_slice<20,16> control_b{0xcafebabe};
  champsim::address_slice<20,16> test_b{0xcafebabe};

  REQUIRE(test_a == control_a);
  REQUIRE(test_b == control_b);

  std::swap(test_a, test_b);

  REQUIRE(test_b == control_a);
  REQUIRE(test_a == control_b);
}

namespace {
  template <typename SliceA, typename SliceB>
    using cmp_slice = decltype( std::declval<SliceA>() == std::declval<SliceB>() );
}

TEST_CASE("Statically sized address slices can be compared if their begins and ends are the same") {
  STATIC_REQUIRE(champsim::is_detected_v<::cmp_slice, champsim::address_slice<16,8>, champsim::address_slice<16,8>>);

  champsim::address addr_a{0xdeadbeef};
  champsim::address addr_b{0xcafefeed};

  REQUIRE(champsim::address_slice<16,8>{addr_a} == champsim::address_slice<16,8>{addr_a});
  REQUIRE_FALSE(champsim::address_slice<16,8>{addr_a} != champsim::address_slice<16,8>{addr_a});
  REQUIRE(champsim::address_slice<16,8>{addr_a} != champsim::address_slice<16,8>{addr_b});
  REQUIRE_FALSE(champsim::address_slice<16,8>{addr_a} == champsim::address_slice<16,8>{addr_b});
}

TEST_CASE("Statically sized address slices can compare equal even if the underlying addresses differ outside the slice") {
  champsim::address addr_a{0xcafefeef};
  champsim::address addr_b{0xcafefeed};

  REQUIRE(champsim::address_slice<16,8>{addr_a} == champsim::address_slice<16,8>{addr_b});
  REQUIRE_FALSE(champsim::address_slice<16,8>{addr_a} != champsim::address_slice<16,8>{addr_b});
  REQUIRE(champsim::address_slice<20,12>{addr_a} == champsim::address_slice<20,12>{addr_b});
  REQUIRE_FALSE(champsim::address_slice<20,12>{addr_a} != champsim::address_slice<20,12>{addr_b});
  REQUIRE(champsim::address_slice<24,16>{addr_a} == champsim::address_slice<24,16>{addr_b});
  REQUIRE_FALSE(champsim::address_slice<24,16>{addr_a} != champsim::address_slice<24,16>{addr_b});
  REQUIRE(champsim::address_slice<28,20>{addr_a} == champsim::address_slice<28,20>{addr_b});
  REQUIRE_FALSE(champsim::address_slice<28,20>{addr_a} != champsim::address_slice<28,20>{addr_b});
  REQUIRE(champsim::address_slice<2,0>{addr_a} != champsim::address_slice<2,0>{addr_b});
  REQUIRE_FALSE(champsim::address_slice<2,0>{addr_a} == champsim::address_slice<2,0>{addr_b});
}

TEST_CASE("Statically-sized address slices cannot be compared if their begins and ends are not the same") {
  STATIC_REQUIRE_FALSE(champsim::is_detected_v<::cmp_slice, champsim::address_slice<20,16>, champsim::address_slice<10,6>>);
  STATIC_REQUIRE_FALSE(champsim::is_detected_v<::cmp_slice, champsim::address_slice<20,16>, champsim::address_slice<20,6>>);
  STATIC_REQUIRE_FALSE(champsim::is_detected_v<::cmp_slice, champsim::address_slice<20,16>, champsim::address_slice<30,16>>);
}

TEST_CASE("Statically-sized address slices can be re-sliced") {
  champsim::address addr{0xabcdef89};

  REQUIRE(champsim::address_slice<20,8>{addr}.slice<8,2>() == champsim::address_slice<16,10>{addr});
}

TEST_CASE("An address slice compares for equality") {
  champsim::address_slice<20,16> lhs{10};
  REQUIRE_FALSE(lhs == champsim::address_slice<20,16>{9});
  REQUIRE(lhs == champsim::address_slice<20,16>{10});
  REQUIRE_FALSE(lhs == champsim::address_slice<20,16>{11});
}

TEST_CASE("An address slice compares for inequality") {
  champsim::address_slice<20,16> lhs{10};
  REQUIRE(lhs != champsim::address_slice<20,16>{9});
  REQUIRE_FALSE(lhs != champsim::address_slice<20,16>{10});
  REQUIRE(lhs != champsim::address_slice<20,16>{11});
}

TEST_CASE("An address slice compares less") {
  champsim::address_slice<20,16> lhs{10};
  REQUIRE_FALSE(lhs < champsim::address_slice<20,16>{9});
  REQUIRE_FALSE(lhs < champsim::address_slice<20,16>{10});
  REQUIRE(lhs < champsim::address_slice<20,16>{11});
}

TEST_CASE("An address slice compares less than or equal") {
  champsim::address_slice<20,16> lhs{10};
  REQUIRE_FALSE(lhs <= champsim::address_slice<20,16>{9});
  REQUIRE(lhs <= champsim::address_slice<20,16>{10});
  REQUIRE(lhs <= champsim::address_slice<20,16>{11});
}

TEST_CASE("An address slice compares greater") {
  champsim::address_slice<20,16> lhs{10};
  REQUIRE_FALSE(lhs < champsim::address_slice<20,16>{9});
  REQUIRE_FALSE(lhs < champsim::address_slice<20,16>{10});
  REQUIRE(lhs < champsim::address_slice<20,16>{11});
}

TEST_CASE("An address slice compares greater than or equal") {
  champsim::address_slice<20,16> lhs{10};
  REQUIRE(lhs >= champsim::address_slice<20,16>{9});
  REQUIRE(lhs >= champsim::address_slice<20,16>{10});
  REQUIRE_FALSE(lhs >= champsim::address_slice<20,16>{11});
}

TEST_CASE("An address slice can add") {
  champsim::address_slice<20,16> lhs{1};
  auto result = lhs + 1;
  REQUIRE(result == champsim::address_slice<20,16>{2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("An address slice can add in place") {
  champsim::address_slice<20,16> lhs{1};
  lhs += 1;
  REQUIRE(lhs == champsim::address_slice<20,16>{2});
  REQUIRE(champsim::address{lhs} == champsim::address{0x20000});
}

TEST_CASE("An address slice can subtract") {
  champsim::address_slice<20,16> lhs{3};
  auto result = lhs - 1;
  REQUIRE(result == champsim::address_slice<20,16>{2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("An address slice can subtract in place") {
  champsim::address_slice<20,16> lhs{3};
  lhs -= 1;
  REQUIRE(lhs == champsim::address_slice<20,16>{2});
  REQUIRE(champsim::address{lhs} == champsim::address{0x20000});
}

TEST_CASE("An address slice can add a negative number") {
  champsim::address_slice<20,16> lhs{3};
  auto result = lhs + -1;
  REQUIRE(result == champsim::address_slice<20,16>{2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("An address slice can add a negative number in place") {
  champsim::address_slice<20,16> lhs{3};
  lhs += -1;
  REQUIRE(lhs == champsim::address_slice<20,16>{2});
  REQUIRE(champsim::address{lhs} == champsim::address{0x20000});
}

TEST_CASE("An address slice can subtract a negative number") {
  champsim::address_slice<20,16> lhs{1};
  auto result = lhs - (-1);
  REQUIRE(result == champsim::address_slice<20,16>{2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("An address slice can subtract a negative number in place") {
  champsim::address_slice<20,16> lhs{1};
  lhs -= -1;
  REQUIRE(lhs == champsim::address_slice<20,16>{2});
  REQUIRE(champsim::address{lhs} == champsim::address{0x20000});
}

TEST_CASE("A const address slice can add") {
  const champsim::address_slice<20,16> lhs{1};
  auto result = lhs + 1;
  REQUIRE(result == champsim::address_slice<20,16>{2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("A const address slice can subtract") {
  const champsim::address_slice<20,16> lhs{3};
  auto result = lhs - 1;
  REQUIRE(result == champsim::address_slice<20,16>{2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("Address slices with the same size can be spliced") {
  champsim::address_slice<20,12> lhs{0xaaa};
  champsim::address_slice<20,12> rhs{0xbbb};

  REQUIRE(champsim::splice(lhs, rhs, 4) == champsim::address_slice<20,12>{0xaab});
  REQUIRE(champsim::splice(lhs, rhs, 8) == champsim::address_slice<20,12>{0xabb});
  REQUIRE(champsim::splice(lhs, rhs, 8, 4) == champsim::address_slice<20,12>{0xaba});
}

TEST_CASE("Address slices with adjacent indices can be spliced") {
  champsim::address_slice<20,12> lhs{0xaaa};
  champsim::address_slice<12,0> rhs{0xbbb};

  REQUIRE(champsim::splice(lhs, rhs) == champsim::address_slice<20,0>{0xaaabbb});
}

TEST_CASE("Address slices that are subsets can be spliced") {
  champsim::address_slice<20,0> lhs{0xaaaaaa};
  champsim::address_slice<16,8> rhs{0xbb};

  REQUIRE(champsim::splice(lhs, rhs) == champsim::address_slice<20,0>{0xaabbaa});
}
