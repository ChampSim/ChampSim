#define CATCH_CONFIG_RUNTIME_STATIC_REQUIRE
#include "catch.hpp"

#include "champsim.h"
#include "address.h"
#include "util/detect.h"

TEST_CASE("An address slice is constructible from a uint64_t") {
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<champsim::static_extent<20,16>>, uint64_t>);
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<champsim::static_extent<64,16>>, uint64_t>);

  auto address = GENERATE(as<uint64_t>{}, 0xdeadbeef, 0xffff'ffff'ffff'ffff);

  champsim::address_slice test_a{champsim::static_extent<20,16>{}, address};
  REQUIRE(test_a.to<uint64_t>() == (address & champsim::bitmask(20-16)));

  champsim::address_slice test_b{champsim::static_extent<64,16>{}, address};
  REQUIRE(test_b.to<uint64_t>() == (address & champsim::bitmask(64-16)));
}

TEMPLATE_TEST_CASE_SIG("An address slice is constexpr constructible from a uint64_t", "", ((uint64_t ADDR), ADDR), 0xdeadbeef, 0xffff'ffff'ffff'ffff) {
  constexpr champsim::address_slice test_a{champsim::static_extent<20,16>{}, ADDR};
  STATIC_REQUIRE(test_a.to<uint64_t>() == (ADDR & champsim::bitmask(20-16)));

  constexpr champsim::address_slice test_b{champsim::static_extent<64,16>{}, ADDR};
  STATIC_REQUIRE(test_b.to<uint64_t>() == (ADDR & champsim::bitmask(64-16)));
}

TEST_CASE("An address slice is constructible from a wider address_slice") {
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<champsim::static_extent<20,16>>, champsim::address_slice<champsim::static_extent<24,12>>>);

  auto addr = GENERATE(champsim::address{0x1234'5678'90ab'cdef}, champsim::address{0xabababab}, champsim::address{0xffff'ffff'ffff'ffff});

  champsim::address_slice control{champsim::static_extent<20,16>{}, addr};
  champsim::address_slice wide{champsim::static_extent<24,12>{}, addr};
  champsim::address_slice narrow{champsim::static_extent<20,16>{}, wide};
  REQUIRE(narrow == control);
}

TEMPLATE_TEST_CASE_SIG("An address slice is constexpr constructible from a wider address_slice", "", ((uint64_t ADDR), ADDR), 0x1234'5678'90ab'cdef, 0xabababab, 0xffff'ffff'ffff'ffff) {
  constexpr champsim::address_slice control{champsim::static_extent<20,16>{}, (ADDR & champsim::bitmask(20,16)) >> 16};
  constexpr champsim::address_slice wide{champsim::static_extent<24,12>{}, (ADDR & champsim::bitmask(24,12)) >> 12};
  constexpr champsim::address_slice narrow{champsim::static_extent<20,16>{}, wide};
  REQUIRE(narrow == control);
}

TEST_CASE("An address slice is constructible from an address_slice that shares a lower endpoint") {
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<champsim::static_extent<20,16>>, champsim::address_slice<champsim::static_extent<24,16>>>);

  auto addr = GENERATE(champsim::address{0x1234'5678'90ab'cdef}, champsim::address{0xabababab}, champsim::address{0xffff'ffff'ffff'ffff});

  champsim::address_slice control{champsim::static_extent<20,16>{}, addr};
  champsim::address_slice wide{champsim::static_extent<24,16>{}, addr};
  champsim::address_slice narrow{champsim::static_extent<20,16>{}, wide};
  REQUIRE(narrow == control);
}

TEST_CASE("An address slice is constructible from an address_slice that shares an upper endpoint") {
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<champsim::static_extent<20,16>>, champsim::address_slice<champsim::static_extent<20,12>>>);

  auto addr = GENERATE(champsim::address{0x1234'5678'90ab'cdef}, champsim::address{0xabababab}, champsim::address{0xffff'ffff'ffff'ffff});

  champsim::address_slice control{champsim::static_extent<20,16>{}, addr};
  champsim::address_slice wide{champsim::static_extent<20,12>{}, addr};
  champsim::address_slice narrow{champsim::static_extent<20,16>{}, wide};
  REQUIRE(narrow == control);
}

TEST_CASE("An address slice is constructible from a narrower address slice") {
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<champsim::static_extent<24,12>>, champsim::address_slice<champsim::static_extent<20,16>>>);

  auto addr = GENERATE(champsim::address{0x1234'5678'90ab'cdef}, champsim::address{0xabababab}, champsim::address{0xffff'ffff'ffff'ffff});

  champsim::address_slice control{champsim::static_extent<24,12>{}, (addr.to<uint64_t>() & champsim::bitmask(20,16)) >> 12};
  champsim::address_slice narrow{champsim::static_extent<20,16>{}, addr};
  champsim::address_slice wide{champsim::static_extent<24,12>{}, narrow};
  REQUIRE(wide == control);
}

TEST_CASE("A static address slice is constructible from a dynamic address slice") {
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<champsim::static_extent<20,12>>, champsim::address_slice<champsim::dynamic_extent>>);

  champsim::address_slice<champsim::dynamic_extent> source{champsim::dynamic_extent{20,12},0xabc};
  champsim::address_slice test_a{champsim::static_extent<20,12>{}, source};
  REQUIRE(test_a.to<uint64_t>() == source.to<uint64_t>());
  REQUIRE(test_a.upper_extent() == source.upper_extent());
  REQUIRE(test_a.lower_extent() == source.lower_extent());
}

TEST_CASE("An address_slice is copy constructible") {
  STATIC_REQUIRE(std::is_copy_constructible_v<champsim::address_slice<champsim::static_extent<20,16>>>);

  champsim::address_slice control{champsim::static_extent<20,16>{}, 0xffff'ffff'ffff'ffff};
  champsim::address_slice test_a{champsim::static_extent<20,16>{}, 0xffff'ffff'ffff'ffff};
  REQUIRE(test_a == control);

  champsim::address_slice test_b{champsim::static_extent<20,16>{}, test_a};
  REQUIRE(test_a == test_b);
}

TEST_CASE("An address_slice is move constructible") {
  STATIC_REQUIRE(std::is_move_constructible_v<champsim::address_slice<champsim::static_extent<20,16>>>);

  champsim::address_slice control{champsim::static_extent<20,16>{}, 0xffff'ffff'ffff'ffff};
  champsim::address_slice test_a{champsim::static_extent<20,16>{}, 0xffff'ffff'ffff'ffff};
  REQUIRE(test_a == control);

  champsim::address_slice test_b{champsim::static_extent<20,16>{}, std::move(test_a)};
  REQUIRE(test_b == control);
}

TEST_CASE("An address_slice is copy assignable") {
  STATIC_REQUIRE(std::is_copy_assignable_v<champsim::address_slice<champsim::static_extent<20,16>>>);

  champsim::address_slice test_a{champsim::static_extent<20,16>{}, 0};
  champsim::address_slice test_b{champsim::static_extent<20,16>{}, 0xffff'ffff'ffff'ffff};
  champsim::address_slice control{champsim::static_extent<20,16>{}, 0xffff'ffff'ffff'ffff};
  REQUIRE(test_b == control);
  REQUIRE(test_a != test_b);

  test_a = test_b;
  REQUIRE(test_a == test_b);
  REQUIRE(test_a == control);
  REQUIRE(test_b == control);
}

TEST_CASE("An address_slice is move assignable") {
  STATIC_REQUIRE(std::is_move_assignable_v<champsim::address_slice<champsim::static_extent<20,16>>>);

  champsim::address_slice control{champsim::static_extent<20,16>{}, 0xffff'ffff'ffff'ffff};
  champsim::address_slice test_a{champsim::static_extent<20,16>{}, 0xffff'ffff'ffff'ffff};
  champsim::address_slice test_b{champsim::static_extent<20,16>{}, 0};
  REQUIRE(test_a == control);
  REQUIRE(test_b != control);

  test_b = std::move(test_a);
  REQUIRE(test_b == control);
}

TEST_CASE("An address_slice is swappable") {
  STATIC_REQUIRE(std::is_swappable_v<champsim::address_slice<champsim::static_extent<20,16>>>);

  champsim::address_slice control_a{champsim::static_extent<20,16>{}, 0xffff'ffff'ffff'ffff};
  champsim::address_slice test_a{champsim::static_extent<20,16>{}, 0xffff'ffff'ffff'ffff};
  champsim::address_slice control_b{champsim::static_extent<20,16>{}, 0xcafebabe};
  champsim::address_slice test_b{champsim::static_extent<20,16>{}, 0xcafebabe};

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
  STATIC_REQUIRE(champsim::is_detected_v<::cmp_slice, champsim::address_slice<champsim::static_extent<16,8>>, champsim::address_slice<champsim::static_extent<16,8>>>);

  champsim::address addr_a{0xdeadbeef};
  champsim::address addr_b{0xcafefeed};

  REQUIRE(champsim::address_slice{champsim::static_extent<16,8>{}, addr_a} == champsim::address_slice{champsim::static_extent<16,8>{}, addr_a});
  REQUIRE_FALSE(champsim::address_slice{champsim::static_extent<16,8>{}, addr_a} != champsim::address_slice{champsim::static_extent<16,8>{}, addr_a});
  REQUIRE(champsim::address_slice{champsim::static_extent<16,8>{}, addr_a} != champsim::address_slice{champsim::static_extent<16,8>{}, addr_b});
  REQUIRE_FALSE(champsim::address_slice{champsim::static_extent<16,8>{}, addr_a} == champsim::address_slice{champsim::static_extent<16,8>{}, addr_b});
}

TEST_CASE("Statically sized address slices can compare equal even if the underlying addresses differ outside the slice") {
  champsim::address addr_a{0xcafefeef};
  champsim::address addr_b{0xcafefeed};

  REQUIRE(champsim::address_slice{champsim::static_extent<16,8>{}, addr_a} == champsim::address_slice{champsim::static_extent<16,8>{}, addr_b});
  REQUIRE_FALSE(champsim::address_slice{champsim::static_extent<16,8>{}, addr_a} != champsim::address_slice{champsim::static_extent<16,8>{}, addr_b});
  REQUIRE(champsim::address_slice{champsim::static_extent<20,12>{}, addr_a} == champsim::address_slice{champsim::static_extent<20,12>{}, addr_b});
  REQUIRE_FALSE(champsim::address_slice{champsim::static_extent<20,12>{}, addr_a} != champsim::address_slice{champsim::static_extent<20,12>{}, addr_b});
  REQUIRE(champsim::address_slice{champsim::static_extent<24,16>{}, addr_a} == champsim::address_slice{champsim::static_extent<24,16>{}, addr_b});
  REQUIRE_FALSE(champsim::address_slice{champsim::static_extent<24,16>{}, addr_a} != champsim::address_slice{champsim::static_extent<24,16>{}, addr_b});
  REQUIRE(champsim::address_slice{champsim::static_extent<28,20>{}, addr_a} == champsim::address_slice{champsim::static_extent<28,20>{}, addr_b});
  REQUIRE_FALSE(champsim::address_slice{champsim::static_extent<28,20>{}, addr_a} != champsim::address_slice{champsim::static_extent<28,20>{}, addr_b});
  REQUIRE(champsim::address_slice{champsim::static_extent<2,0>{}, addr_a} != champsim::address_slice{champsim::static_extent<2,0>{}, addr_b});
  REQUIRE_FALSE(champsim::address_slice{champsim::static_extent<2,0>{}, addr_a} == champsim::address_slice{champsim::static_extent<2,0>{}, addr_b});
}

TEST_CASE("Statically-sized address slices cannot be compared if their begins and ends are not the same") {
  STATIC_REQUIRE_FALSE(champsim::is_detected_v<::cmp_slice, champsim::address_slice<champsim::static_extent<20,16>>, champsim::address_slice<champsim::static_extent<10,6>>>);
  STATIC_REQUIRE_FALSE(champsim::is_detected_v<::cmp_slice, champsim::address_slice<champsim::static_extent<20,16>>, champsim::address_slice<champsim::static_extent<20,6>>>);
  STATIC_REQUIRE_FALSE(champsim::is_detected_v<::cmp_slice, champsim::address_slice<champsim::static_extent<20,16>>, champsim::address_slice<champsim::static_extent<30,16>>>);
}

TEST_CASE("Statically-sized address slices can be re-sliced") {
  champsim::address addr{0xabcdef89};

  REQUIRE(champsim::address_slice{champsim::static_extent<20,8>{}, addr}.slice(champsim::static_extent<8,2>{}) == champsim::address_slice{champsim::static_extent<16,10>{}, addr});
  REQUIRE(champsim::address_slice{champsim::static_extent<20,8>{}, addr}.slice<8,2>() == champsim::address_slice{champsim::static_extent<16,10>{}, addr});
}

TEST_CASE("An address slice compares for equality") {
  champsim::address_slice lhs{champsim::static_extent<20,16>{}, 10};
  REQUIRE_FALSE(lhs == champsim::address_slice{champsim::static_extent<20,16>{}, 9});
  REQUIRE(lhs == champsim::address_slice{champsim::static_extent<20,16>{}, 10});
  REQUIRE_FALSE(lhs == champsim::address_slice{champsim::static_extent<20,16>{}, 11});
}

TEST_CASE("An address slice compares for inequality") {
  champsim::address_slice lhs{champsim::static_extent<20,16>{}, 10};
  REQUIRE(lhs != champsim::address_slice{champsim::static_extent<20,16>{}, 9});
  REQUIRE_FALSE(lhs != champsim::address_slice{champsim::static_extent<20,16>{}, 10});
  REQUIRE(lhs != champsim::address_slice{champsim::static_extent<20,16>{}, 11});
}

TEST_CASE("An address slice compares less") {
  champsim::address_slice lhs{champsim::static_extent<20,16>{}, 10};
  REQUIRE_FALSE(lhs < champsim::address_slice{champsim::static_extent<20,16>{}, 9});
  REQUIRE_FALSE(lhs < champsim::address_slice{champsim::static_extent<20,16>{}, 10});
  REQUIRE(lhs < champsim::address_slice{champsim::static_extent<20,16>{}, 11});
}

TEST_CASE("An address slice compares less than or equal") {
  champsim::address_slice lhs{champsim::static_extent<20,16>{}, 10};
  REQUIRE_FALSE(lhs <= champsim::address_slice{champsim::static_extent<20,16>{}, 9});
  REQUIRE(lhs <= champsim::address_slice{champsim::static_extent<20,16>{}, 10});
  REQUIRE(lhs <= champsim::address_slice{champsim::static_extent<20,16>{}, 11});
}

TEST_CASE("An address slice compares greater") {
  champsim::address_slice lhs{champsim::static_extent<20,16>{}, 10};
  REQUIRE_FALSE(lhs < champsim::address_slice{champsim::static_extent<20,16>{}, 9});
  REQUIRE_FALSE(lhs < champsim::address_slice{champsim::static_extent<20,16>{}, 10});
  REQUIRE(lhs < champsim::address_slice{champsim::static_extent<20,16>{}, 11});
}

TEST_CASE("An address slice compares greater than or equal") {
  champsim::address_slice lhs{champsim::static_extent<20,16>{}, 10};
  REQUIRE(lhs >= champsim::address_slice{champsim::static_extent<20,16>{}, 9});
  REQUIRE(lhs >= champsim::address_slice{champsim::static_extent<20,16>{}, 10});
  REQUIRE_FALSE(lhs >= champsim::address_slice{champsim::static_extent<20,16>{}, 11});
}

TEST_CASE("An address slice can add") {
  champsim::address_slice lhs{champsim::static_extent<20,16>{}, 1};
  auto result = lhs + 1;
  REQUIRE(result == champsim::address_slice{champsim::static_extent<20,16>{}, 2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("An address slice can add in place") {
  champsim::address_slice lhs{champsim::static_extent<20,16>{}, 1};
  lhs += 1;
  REQUIRE(lhs == champsim::address_slice{champsim::static_extent<20,16>{}, 2});
  REQUIRE(champsim::address{lhs} == champsim::address{0x20000});
}

TEST_CASE("An address slice can subtract") {
  champsim::address_slice lhs{champsim::static_extent<20,16>{}, 3};
  auto result = lhs - 1;
  REQUIRE(result == champsim::address_slice{champsim::static_extent<20,16>{}, 2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("An address slice can subtract in place") {
  champsim::address_slice lhs{champsim::static_extent<20,16>{}, 3};
  lhs -= 1;
  REQUIRE(lhs == champsim::address_slice{champsim::static_extent<20,16>{}, 2});
  REQUIRE(champsim::address{lhs} == champsim::address{0x20000});
}

TEST_CASE("An address slice can add a negative number") {
  champsim::address_slice lhs{champsim::static_extent<20,16>{}, 3};
  auto result = lhs + -1;
  REQUIRE(result == champsim::address_slice{champsim::static_extent<20,16>{}, 2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("An address slice can add a negative number in place") {
  champsim::address_slice lhs{champsim::static_extent<20,16>{}, 3};
  lhs += -1;
  REQUIRE(lhs == champsim::address_slice{champsim::static_extent<20,16>{}, 2});
  REQUIRE(champsim::address{lhs} == champsim::address{0x20000});
}

TEST_CASE("An address slice can subtract a negative number") {
  champsim::address_slice lhs{champsim::static_extent<20,16>{}, 1};
  auto result = lhs - (-1);
  REQUIRE(result == champsim::address_slice{champsim::static_extent<20,16>{}, 2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("An address slice can subtract a negative number in place") {
  champsim::address_slice lhs{champsim::static_extent<20,16>{}, 1};
  lhs -= -1;
  REQUIRE(lhs == champsim::address_slice{champsim::static_extent<20,16>{}, 2});
  REQUIRE(champsim::address{lhs} == champsim::address{0x20000});
}

TEST_CASE("A const address slice can add") {
  const champsim::address_slice lhs{champsim::static_extent<20,16>{}, 1};
  auto result = lhs + 1;
  REQUIRE(result == champsim::address_slice{champsim::static_extent<20,16>{}, 2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("A const address slice can subtract") {
  const champsim::address_slice lhs{champsim::static_extent<20,16>{}, 3};
  auto result = lhs - 1;
  REQUIRE(result == champsim::address_slice{champsim::static_extent<20,16>{}, 2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("An address slice can pre-increment") {
  champsim::address_slice lhs{champsim::static_extent<20,16>{}, 1};
  auto result = ++lhs;
  REQUIRE(result == champsim::address_slice{champsim::static_extent<20,16>{}, 2});
  REQUIRE(lhs == champsim::address_slice{champsim::static_extent<20,16>{}, 2});
}

TEST_CASE("An address slice can post-increment") {
  champsim::address_slice lhs{champsim::static_extent<20,16>{}, 1};
  auto result = lhs++;
  REQUIRE(result == champsim::address_slice{champsim::static_extent<20,16>{}, 1});
  REQUIRE(lhs == champsim::address_slice{champsim::static_extent<20,16>{}, 2});
}

TEST_CASE("An address slice can pre-decrement") {
  champsim::address_slice lhs{champsim::static_extent<20,16>{}, 2};
  auto result = --lhs;
  REQUIRE(result == champsim::address_slice{champsim::static_extent<20,16>{}, 1});
  REQUIRE(lhs == champsim::address_slice{champsim::static_extent<20,16>{}, 1});
}

TEST_CASE("An address slice can post-decrement") {
  champsim::address_slice lhs{champsim::static_extent<20,16>{}, 2};
  auto result = lhs--;
  REQUIRE(result == champsim::address_slice{champsim::static_extent<20,16>{}, 2});
  REQUIRE(lhs == champsim::address_slice{champsim::static_extent<20,16>{}, 1});
}

TEMPLATE_TEST_CASE_SIG("Address slices with adjacent indices can be spliced", "", ((std::size_t V, std::size_t W), V, W), (8,4), (12,4), (16,4), (20,4), (24,4), (28,4), (12,8), (16,8), (20,8), (24,8), (28,8), (16,12), (20,12), (24,12), (28,12), (20,16), (24,16), (28,16),  (24,20), (28,20), (28,24)) {
  champsim::address_slice lhs{champsim::static_extent<32,V>{}, 0xaaaa'aaaa};
  champsim::address_slice rhs{champsim::static_extent<V,W>{}, 0xbbbb'bbbb};

  REQUIRE(champsim::splice(lhs, rhs).upper_extent() == 32);
  REQUIRE(champsim::splice(lhs, rhs).lower_extent() == W);
  auto raw_val = champsim::splice_bits(0xaaaa'aaaa, 0xbbbb'bbbb, V, W) >> W;
  REQUIRE(champsim::splice(lhs, rhs) == champsim::address_slice{champsim::static_extent<32,W>{}, raw_val});
}

TEMPLATE_TEST_CASE_SIG("Address slices that are subsets can be spliced", "", ((std::size_t V, std::size_t W), V, W), (8,4), (12,4), (16,4), (20,4), (24,4), (28,4), (12,8), (16,8), (20,8), (24,8), (28,8), (16,12), (20,12), (24,12), (28,12), (20,16), (24,16), (28,16),  (24,20), (28,20), (28,24)) {
  champsim::address_slice lhs{champsim::static_extent<32,0>{}, 0xaaaa'aaaa};
  champsim::address_slice rhs{champsim::static_extent<V,W>{}, 0xbbbb'bbbb};

  REQUIRE(champsim::splice(lhs, rhs).upper_extent() == 32);
  REQUIRE(champsim::splice(lhs, rhs).lower_extent() == 0);
  REQUIRE(champsim::splice(lhs, rhs) == champsim::address_slice{champsim::static_extent<32,0>{}, champsim::splice_bits(0xaaaa'aaaa, 0xbbbb'bbbb, V, W)});
}
