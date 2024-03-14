#define CATCH_CONFIG_RUNTIME_STATIC_REQUIRE
#include "catch.hpp"

#include "champsim.h"
#include "address.h"

#include <type_traits>

using namespace champsim::data::data_literals;

TEST_CASE("A dynamic address slice is constructible from a static address slice") {
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<champsim::dynamic_extent>, champsim::address_slice<champsim::static_extent<20_b,12_b>>>);

  champsim::address_slice source{champsim::static_extent<20_b,12_b>{}, 0xabc};
  champsim::address_slice<champsim::dynamic_extent> test_a{source};
  REQUIRE(test_a.to<uint64_t>() == source.to<uint64_t>());
  REQUIRE(test_a.upper_extent() == source.upper_extent());
  REQUIRE(test_a.lower_extent() == source.lower_extent());
}

TEST_CASE("A dynamic address slice takes constructor arguments that affect slice positioning") {
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<champsim::dynamic_extent>, uint64_t>);

  champsim::address_slice test_a{champsim::dynamic_extent{4_b,0_b},0xf};
  REQUIRE(champsim::address{test_a} == champsim::address{0xf});

  champsim::address_slice test_b{champsim::dynamic_extent{12_b,4_b},0xee};
  REQUIRE(champsim::address{test_b} == champsim::address{0xee0});

  champsim::address_slice test_c{champsim::dynamic_extent{24_b,12_b},0xadb};
  REQUIRE(champsim::address{test_c} == champsim::address{0xadb000});
}

TEST_CASE("A dynamic address slice takes constructor arguments that affect slicing") {
  champsim::address address{0xdeadbeef};
  STATIC_REQUIRE(std::is_constructible_v<champsim::address_slice<champsim::dynamic_extent>, champsim::address>);

  champsim::address_slice test_a{champsim::dynamic_extent{4_b,0_b},address};
  REQUIRE(test_a.to<uint64_t>() == 0xf);

  champsim::address_slice test_b{champsim::dynamic_extent{12_b,4_b},address};
  REQUIRE(test_b.to<uint64_t>() == 0xee);

  champsim::address_slice test_c{champsim::dynamic_extent{24_b,12_b},address};
  REQUIRE(test_c.to<uint64_t>() == 0xadb);
}

TEST_CASE("A dynamic address_slice is copy constructible") {
  STATIC_REQUIRE(std::is_copy_constructible_v<champsim::address_slice<champsim::dynamic_extent>>);

  champsim::dynamic_extent ext{64_b,0_b};
  champsim::address_slice control{ext, 0xdeadbeef};
  champsim::address_slice test_a{ext, 0xdeadbeef};
  REQUIRE(test_a == control);

  champsim::address_slice test_b{ext, test_a};
  REQUIRE(test_a == test_b);
}

TEST_CASE("A dynamic address_slice is move constructible") {
  STATIC_REQUIRE(std::is_move_constructible_v<champsim::address_slice<champsim::dynamic_extent>>);

  champsim::dynamic_extent ext{64_b,0_b};
  champsim::address_slice control{ext, 0xdeadbeef};
  champsim::address_slice test_a{ext, 0xdeadbeef};
  REQUIRE(test_a == control);

  champsim::address_slice test_b{ext, std::move(test_a)};
  REQUIRE(test_b == control);
}

TEST_CASE("A dynamic address_slice is copy assignable") {
  STATIC_REQUIRE(std::is_copy_assignable_v<champsim::address_slice<champsim::dynamic_extent>>);

  champsim::dynamic_extent ext{64_b,0_b};
  champsim::address_slice test_a{ext, 0};
  champsim::address_slice test_b{ext, 0xdeadbeef};
  champsim::address_slice control{ext, 0xdeadbeef};
  REQUIRE(test_b == control);
  REQUIRE(test_a != test_b);

  test_a = test_b;
  CHECK(test_a == test_b);
  CHECK(test_a == control);
  CHECK(test_b == control);
}

TEST_CASE("A dynamic address_slice is move assignable") {
  STATIC_REQUIRE(std::is_move_assignable_v<champsim::address_slice<champsim::dynamic_extent>>);

  champsim::dynamic_extent ext{64_b,0_b};
  champsim::address_slice control{ext, 0xdeadbeef};
  champsim::address_slice test_a{ext, 0xdeadbeef};
  champsim::address_slice test_b{ext, 0};
  REQUIRE(test_a == control);
  REQUIRE(test_b != control);

  test_b = std::move(test_a);
  REQUIRE(test_b == control);
}

TEST_CASE("A dynamic address_slice is swappable") {
  STATIC_REQUIRE(std::is_swappable_v<champsim::address_slice<champsim::dynamic_extent>>);

  champsim::dynamic_extent ext{64_b,0_b};
  champsim::address_slice control_a{ext, 0xdeadbeef};
  champsim::address_slice test_a{ext, 0xdeadbeef};
  champsim::address_slice control_b{ext, 0xcafebabe};
  champsim::address_slice test_b{ext, 0xcafebabe};

  REQUIRE(test_a == control_a);
  REQUIRE(test_b == control_b);

  std::swap(test_a, test_b);

  REQUIRE(test_b == control_a);
  REQUIRE(test_a == control_b);
}

TEST_CASE("Dynamically-sized address slices can be compared if their begins and ends are the same") {
  champsim::address addr_a{0xdeadbeef};
  champsim::address addr_b{0xcafefeed};

  REQUIRE(champsim::address_slice{champsim::dynamic_extent{16_b,8_b},addr_a} == champsim::address_slice{champsim::dynamic_extent{16_b,8_b},addr_a});
  REQUIRE_FALSE(champsim::address_slice{champsim::dynamic_extent{16_b,8_b},addr_a} != champsim::address_slice{champsim::dynamic_extent{16_b,8_b},addr_a});
  REQUIRE(champsim::address_slice{champsim::dynamic_extent{16_b,8_b},addr_a} != champsim::address_slice{champsim::dynamic_extent{16_b,8_b},addr_b});
  REQUIRE_FALSE(champsim::address_slice{champsim::dynamic_extent{16_b,8_b},addr_a} == champsim::address_slice{champsim::dynamic_extent{16_b,8_b},addr_b});
}

TEST_CASE("Dynamically-sized address slices can compare equal even if the underlying addresses differ outside the slice") {
  champsim::address addr_a{0xcafefeef};
  champsim::address addr_b{0xcafefeed};

  REQUIRE(champsim::address_slice{champsim::dynamic_extent{16_b,8_b},addr_a} == champsim::address_slice{champsim::dynamic_extent{16_b,8_b},addr_b});
  REQUIRE_FALSE(champsim::address_slice{champsim::dynamic_extent{16_b,8_b},addr_a} != champsim::address_slice{champsim::dynamic_extent{16_b,8_b},addr_b});
  REQUIRE(champsim::address_slice{champsim::dynamic_extent{20_b,12_b},addr_a} == champsim::address_slice{champsim::dynamic_extent{20_b,12_b},addr_b});
  REQUIRE_FALSE(champsim::address_slice{champsim::dynamic_extent{20_b,12_b},addr_a} != champsim::address_slice{champsim::dynamic_extent{20_b,12_b},addr_b});
  REQUIRE(champsim::address_slice{champsim::dynamic_extent{24_b,16_b},addr_a} == champsim::address_slice{champsim::dynamic_extent{24_b,16_b},addr_b});
  REQUIRE_FALSE(champsim::address_slice{champsim::dynamic_extent{24_b,16_b},addr_a} != champsim::address_slice{champsim::dynamic_extent{24_b,16_b},addr_b});
  REQUIRE(champsim::address_slice{champsim::dynamic_extent{28_b,20_b},addr_a} == champsim::address_slice{champsim::dynamic_extent{28_b,20_b},addr_b});
  REQUIRE_FALSE(champsim::address_slice{champsim::dynamic_extent{28_b,20_b},addr_a} != champsim::address_slice{champsim::dynamic_extent{28_b,20_b},addr_b});
  REQUIRE(champsim::address_slice{champsim::dynamic_extent{2_b,0_b},addr_a} != champsim::address_slice{champsim::dynamic_extent{2_b,0_b},addr_b});
  REQUIRE_FALSE(champsim::address_slice{champsim::dynamic_extent{2_b,0_b},addr_a} == champsim::address_slice{champsim::dynamic_extent{2_b,0_b},addr_b});
}

TEST_CASE("Dynamically-sized address sliced can be re-sliced") {
  champsim::address addr{0xabcdef89};

  REQUIRE(champsim::address_slice{champsim::dynamic_extent{20_b,8_b},addr}.slice(champsim::dynamic_extent{8_b,2_b}) == champsim::address_slice{champsim::dynamic_extent{16_b,10_b},addr});
}

TEST_CASE("Dynamically-sized address slices can be split") {
  champsim::address_slice addr{champsim::dynamic_extent{32_b,0_b}, 0xabcdef89};

  auto [up, low] = addr.split(12_b);

  REQUIRE(up == champsim::address_slice{champsim::dynamic_extent{32_b,12_b}, addr});
  REQUIRE(low == champsim::address_slice{champsim::dynamic_extent{12_b,0_b}, addr});
}

TEST_CASE("A dynamically-sized address slice compares for equality") {
  champsim::address_slice lhs{champsim::dynamic_extent{20_b,16_b},10};
  REQUIRE_FALSE(lhs == champsim::address_slice{champsim::dynamic_extent{20_b,16_b},9});
  REQUIRE(lhs == champsim::address_slice{champsim::dynamic_extent{20_b,16_b},10});
  REQUIRE_FALSE(lhs == champsim::address_slice{champsim::dynamic_extent{20_b,16_b},11});
}

TEST_CASE("A dynamically-sized address slice compares for inequality") {
  champsim::address_slice lhs{champsim::dynamic_extent{20_b,16_b},10};
  REQUIRE(lhs != champsim::address_slice{champsim::dynamic_extent{20_b,16_b},9});
  REQUIRE_FALSE(lhs != champsim::address_slice{champsim::dynamic_extent{20_b,16_b},10});
  REQUIRE(lhs != champsim::address_slice{champsim::dynamic_extent{20_b,16_b},11});
}

TEST_CASE("A dynamically-sized address slice compares less") {
  champsim::address_slice lhs{champsim::dynamic_extent{20_b,16_b},10};
  REQUIRE_FALSE(lhs < champsim::address_slice{champsim::dynamic_extent{20_b,16_b},9});
  REQUIRE_FALSE(lhs < champsim::address_slice{champsim::dynamic_extent{20_b,16_b},10});
  REQUIRE(lhs < champsim::address_slice{champsim::dynamic_extent{20_b,16_b},11});
}

TEST_CASE("A dynamically-sized address slice compares less than or equal") {
  champsim::address_slice lhs{champsim::dynamic_extent{20_b,16_b},10};
  REQUIRE_FALSE(lhs <= champsim::address_slice{champsim::dynamic_extent{20_b,16_b},9});
  REQUIRE(lhs <= champsim::address_slice{champsim::dynamic_extent{20_b,16_b},10});
  REQUIRE(lhs <= champsim::address_slice{champsim::dynamic_extent{20_b,16_b},11});
}

TEST_CASE("A dynamically-sized address slice compares greater") {
  champsim::address_slice lhs{champsim::dynamic_extent{20_b,16_b},10};
  REQUIRE_FALSE(lhs < champsim::address_slice{champsim::dynamic_extent{20_b,16_b},9});
  REQUIRE_FALSE(lhs < champsim::address_slice{champsim::dynamic_extent{20_b,16_b},10});
  REQUIRE(lhs < champsim::address_slice{champsim::dynamic_extent{20_b,16_b},11});
}

TEST_CASE("A dynamically-sized address slice compares greater than or equal") {
  champsim::address_slice lhs{champsim::dynamic_extent{20_b,16_b},10};
  REQUIRE(lhs >= champsim::address_slice{champsim::dynamic_extent{20_b,16_b},9});
  REQUIRE(lhs >= champsim::address_slice{champsim::dynamic_extent{20_b,16_b},10});
  REQUIRE_FALSE(lhs >= champsim::address_slice{champsim::dynamic_extent{20_b,16_b},11});
}

TEST_CASE("A dynamically-sized address slice can add") {
  champsim::address_slice lhs{champsim::dynamic_extent{20_b,16_b},1};
  auto result = lhs + 1;
  REQUIRE(result == champsim::address_slice{champsim::dynamic_extent{20_b,16_b},2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("A dynamically-sized address slice can add in place") {
  champsim::address_slice lhs{champsim::dynamic_extent{20_b,16_b},1};
  lhs += 1;
  REQUIRE(lhs == champsim::address_slice{champsim::dynamic_extent{20_b,16_b},2});
  REQUIRE(champsim::address{lhs} == champsim::address{0x20000});
}

TEST_CASE("A dynamically-sized address slice can subtract") {
  champsim::address_slice lhs{champsim::dynamic_extent{20_b,16_b},3};
  auto result = lhs - 1;
  REQUIRE(result == champsim::address_slice{champsim::dynamic_extent{20_b,16_b},2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("A dynamically-sized address slice can subtract in place") {
  champsim::address_slice lhs{champsim::dynamic_extent{20_b,16_b},3};
  lhs -= 1;
  REQUIRE(lhs == champsim::address_slice{champsim::dynamic_extent{20_b,16_b},2});
  REQUIRE(champsim::address{lhs} == champsim::address{0x20000});
}

TEST_CASE("A dynamically-sized address slice can add a negative number") {
  champsim::address_slice lhs{champsim::dynamic_extent{20_b,16_b},3};
  auto result = lhs + -1;
  REQUIRE(result == champsim::address_slice{champsim::dynamic_extent{20_b,16_b},2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("A dynamically-sized address slice can add a negative number in place") {
  champsim::address_slice lhs{champsim::dynamic_extent{20_b,16_b},3};
  lhs += -1;
  REQUIRE(lhs == champsim::address_slice{champsim::dynamic_extent{20_b,16_b},2});
  REQUIRE(champsim::address{lhs} == champsim::address{0x20000});
}

TEST_CASE("A dynamically-sized address slice can subtract a negative number") {
  champsim::address_slice lhs{champsim::dynamic_extent{20_b,16_b},1};
  auto result = lhs - (-1);
  REQUIRE(result == champsim::address_slice{champsim::dynamic_extent{20_b,16_b},2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("A dynamically-sized address slice can subtract a negative number in place") {
  champsim::address_slice lhs{champsim::dynamic_extent{20_b,16_b},1};
  lhs -= -1;
  REQUIRE(lhs == champsim::address_slice{champsim::dynamic_extent{20_b,16_b},2});
  REQUIRE(champsim::address{lhs} == champsim::address{0x20000});
}

TEST_CASE("A const dynamically-sized address slice can add") {
  const champsim::address_slice lhs{champsim::dynamic_extent{20_b,16_b},1};
  auto result = lhs + 1;
  REQUIRE(result == champsim::address_slice{champsim::dynamic_extent{20_b,16_b},2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("A const dynamically-sized address slice can subtract") {
  const champsim::address_slice lhs{champsim::dynamic_extent{20_b,16_b},3};
  auto result = lhs - 1;
  REQUIRE(result == champsim::address_slice{champsim::dynamic_extent{20_b,16_b},2});
  REQUIRE(champsim::address{result} == champsim::address{0x20000});
}

TEST_CASE("An dynamically-sized address slice can pre-increment") {
  champsim::address_slice lhs{champsim::dynamic_extent{20_b,16_b},1};
  auto result = ++lhs;
  REQUIRE(result == champsim::address_slice{champsim::dynamic_extent{20_b,16_b},2});
  REQUIRE(lhs == champsim::address_slice{champsim::dynamic_extent{20_b,16_b},2});
}

TEST_CASE("An dynamically-sized address slice can post-increment") {
  champsim::address_slice lhs{champsim::dynamic_extent{20_b,16_b},1};
  auto result = lhs++;
  REQUIRE(result == champsim::address_slice{champsim::dynamic_extent{20_b,16_b},1});
  REQUIRE(lhs == champsim::address_slice{champsim::dynamic_extent{20_b,16_b},2});
}

TEST_CASE("An dynamically-sized address slice can pre-decrement") {
  champsim::address_slice lhs{champsim::dynamic_extent{20_b,16_b},2};
  auto result = --lhs;
  REQUIRE(result == champsim::address_slice{champsim::dynamic_extent{20_b,16_b},1});
  REQUIRE(lhs == champsim::address_slice{champsim::dynamic_extent{20_b,16_b},1});
}

TEST_CASE("An dynamically-sized address slice can post-decrement") {
  champsim::address_slice lhs{champsim::dynamic_extent{20_b,16_b},2};
  auto result = lhs--;
  REQUIRE(result == champsim::address_slice{champsim::dynamic_extent{20_b,16_b},2});
  REQUIRE(lhs == champsim::address_slice{champsim::dynamic_extent{20_b,16_b},1});
}

TEST_CASE("Dynamic address slices with adjacent indices can be spliced") {
  auto low = GENERATE(4_b,8_b,12_b,16_b,20_b,28_b);
  champsim::address_slice lhs{champsim::dynamic_extent{32_b,low},0xaaaa'aaaa};
  champsim::address_slice rhs{champsim::dynamic_extent{low,0_b},0xbbbb'bbbb};

  REQUIRE(champsim::splice(lhs, rhs) == champsim::address_slice{champsim::dynamic_extent{32_b,0_b},champsim::splice_bits(0xaaaa'aaaa, 0xbbbb'bbbb, low)});
}

TEST_CASE("Dynamic address slices that are subsets can be spliced") {
  auto ext = GENERATE(as<champsim::dynamic_extent>{},
      champsim::dynamic_extent{8_b,4_b}, champsim::dynamic_extent{12_b,4_b}, champsim::dynamic_extent{16_b,4_b}, champsim::dynamic_extent{20_b,4_b}, champsim::dynamic_extent{24_b,4_b}, champsim::dynamic_extent{28_b,4_b},
      champsim::dynamic_extent{12_b,8_b}, champsim::dynamic_extent{16_b,8_b}, champsim::dynamic_extent{20_b,8_b}, champsim::dynamic_extent{24_b,8_b}, champsim::dynamic_extent{28_b,8_b},
      champsim::dynamic_extent{16_b,12_b}, champsim::dynamic_extent{20_b,12_b}, champsim::dynamic_extent{24_b,12_b}, champsim::dynamic_extent{28_b,12_b},
      champsim::dynamic_extent{20_b,16_b}, champsim::dynamic_extent{24_b,16_b}, champsim::dynamic_extent{28_b,16_b},
      champsim::dynamic_extent{24_b,20_b}, champsim::dynamic_extent{28_b,20_b},
      champsim::dynamic_extent{28_b,24_b});
  champsim::address_slice lhs{champsim::dynamic_extent{32_b,0_b},0xaaaa'aaaa};
  champsim::address_slice rhs{ext,0xbbbb'bbbb};

  REQUIRE(champsim::splice(lhs, rhs) == champsim::address_slice{champsim::dynamic_extent{32_b,0_b},champsim::splice_bits(0xaaaa'aaaa, 0xbbbb'bbbb, ext.upper, ext.lower)});
}

