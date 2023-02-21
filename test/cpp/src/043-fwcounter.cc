#include <catch.hpp>
#include "msl/fwcounter.h"

#include <type_traits>

TEMPLATE_TEST_CASE("A fixed-width counter compares for equality", "", champsim::msl::fwcounter<8>, champsim::msl::sfwcounter<8>) {
  TestType lhs{1};
  REQUIRE_FALSE(lhs == 0);
  REQUIRE(lhs == 1);
  REQUIRE_FALSE(lhs == 2);
}

TEMPLATE_TEST_CASE("A fixed-width counter compares for inequality", "", champsim::msl::fwcounter<8>, champsim::msl::sfwcounter<8>) {
  TestType lhs{1};
  REQUIRE(lhs != 0);
  REQUIRE_FALSE(lhs != 1);
  REQUIRE(lhs != 2);
}

TEMPLATE_TEST_CASE("A fixed-width counter compares less", "", champsim::msl::fwcounter<8>, champsim::msl::sfwcounter<8>) {
  TestType lhs{1};
  REQUIRE(lhs < 2);
  REQUIRE_FALSE(lhs < 1);
  REQUIRE_FALSE(lhs < 0);
}

TEMPLATE_TEST_CASE("A fixed-width counter compares greater", "", champsim::msl::fwcounter<8>, champsim::msl::sfwcounter<8>) {
  TestType lhs{1};
  REQUIRE(lhs > 0);
  REQUIRE_FALSE(lhs > 1);
  REQUIRE_FALSE(lhs > 2);
}

TEMPLATE_TEST_CASE("A fixed-width counter compares less or equal", "", champsim::msl::fwcounter<8>, champsim::msl::sfwcounter<8>) {
  TestType lhs{1};
  REQUIRE_FALSE(lhs <= 0);
  REQUIRE(lhs <= 1);
  REQUIRE(lhs <= 2);
}

TEMPLATE_TEST_CASE("A fixed-width counter compares greater or equal", "", champsim::msl::fwcounter<8>, champsim::msl::sfwcounter<8>) {
  TestType lhs{1};
  REQUIRE(lhs >= 0);
  REQUIRE(lhs >= 1);
  REQUIRE_FALSE(lhs >= 2);
}

TEMPLATE_TEST_CASE("A fixed-width counter can add", "", champsim::msl::fwcounter<8>, champsim::msl::sfwcounter<8>) {
  TestType lhs{1};
  auto result = lhs + 1;
  REQUIRE(result.value() == 2);
}

TEMPLATE_TEST_CASE("A fixed-width counter can add in place", "", champsim::msl::fwcounter<8>, champsim::msl::sfwcounter<8>) {
  TestType lhs{1};
  lhs += 1;
  REQUIRE(lhs.value() == 2);
}

TEMPLATE_TEST_CASE("A fixed-width counter can subtract", "", champsim::msl::fwcounter<8>, champsim::msl::sfwcounter<8>) {
  TestType lhs{1};
  auto result = lhs - 1;
  REQUIRE(result.value() == 0);
}

TEMPLATE_TEST_CASE("A fixed-width counter can subtract in place", "", champsim::msl::fwcounter<8>, champsim::msl::sfwcounter<8>) {
  TestType lhs{1};
  lhs -= 1;
  REQUIRE(lhs.value() == 0);
}

TEMPLATE_TEST_CASE("A fixed-width counter can multiply", "", champsim::msl::fwcounter<8>, champsim::msl::sfwcounter<8>) {
  TestType lhs{2};
  auto result = lhs * 2;
  REQUIRE(result.value() == 4);
}

TEMPLATE_TEST_CASE("A fixed-width counter can multiply in place", "", champsim::msl::fwcounter<8>, champsim::msl::sfwcounter<8>) {
  TestType lhs{2};
  lhs *= 2;
  REQUIRE(lhs.value() == 4);
}

TEMPLATE_TEST_CASE("A fixed-width counter can divide", "", champsim::msl::fwcounter<8>, champsim::msl::sfwcounter<8>) {
  TestType lhs{4};
  auto result = lhs / 2;
  REQUIRE(result.value() == 2);
}

TEMPLATE_TEST_CASE("A fixed-width counter can divide in place", "", champsim::msl::fwcounter<8>, champsim::msl::sfwcounter<8>) {
  TestType lhs{4};
  lhs /= 2;
  REQUIRE(lhs.value() == 2);
}

TEMPLATE_TEST_CASE("A fixed-width counter can add a negative number", "", champsim::msl::fwcounter<8>, champsim::msl::sfwcounter<8>) {
  TestType lhs{1};
  auto result = lhs + -1;
  REQUIRE(result.value() == 0);
}

TEMPLATE_TEST_CASE("A fixed-width counter can add a negative number in place", "", champsim::msl::fwcounter<8>, champsim::msl::sfwcounter<8>) {
  TestType lhs{1};
  lhs += -1;
  REQUIRE(lhs.value() == 0);
}

TEMPLATE_TEST_CASE("A fixed-width counter can subtract a negative number", "", champsim::msl::fwcounter<8>, champsim::msl::sfwcounter<8>) {
  TestType lhs{1};
  auto result = lhs - (-1);
  REQUIRE(result.value() == 2);
}

TEMPLATE_TEST_CASE("A fixed-width counter can subtract a negative number in place", "", champsim::msl::fwcounter<8>, champsim::msl::sfwcounter<8>) {
  TestType lhs{1};
  lhs -= -1;
  REQUIRE(lhs.value() == 2);
}

TEMPLATE_TEST_CASE("A fixed-width counter saturates with addition", "", champsim::msl::fwcounter<2>, champsim::msl::sfwcounter<2>) {
  TestType lhs{1};
  lhs += 3*lhs.maximum;
  REQUIRE(lhs.value() == lhs.maximum);
}

TEMPLATE_TEST_CASE("A fixed-width counter saturates with subtraction", "", champsim::msl::fwcounter<2>, champsim::msl::sfwcounter<2>) {
  TestType lhs{1};
  lhs -= 3*lhs.maximum;
  REQUIRE(lhs.value() == lhs.minimum);
}

TEMPLATE_TEST_CASE("A fixed-width counter saturates with multiplication", "", champsim::msl::fwcounter<2>, champsim::msl::sfwcounter<2>) {
  TestType lhs{2};
  lhs *= lhs.maximum;
  REQUIRE(lhs.value() == lhs.maximum);
}

TEMPLATE_PRODUCT_TEST_CASE("A fixed-width counter is constructible by certian means", "",
    (std::is_default_constructible, std::is_copy_constructible, std::is_move_constructible, std::is_copy_assignable, std::is_move_assignable, std::is_destructible, std::is_swappable),
    (champsim::msl::fwcounter<8>, champsim::msl::sfwcounter<8>)) {
  STATIC_REQUIRE(TestType::value);
}

TEMPLATE_TEST_CASE("A fixed-width counter is assignable by certian means", "", champsim::msl::fwcounter<8>, champsim::msl::sfwcounter<8>) {
  STATIC_REQUIRE(std::is_assignable_v<TestType, char>);
  STATIC_REQUIRE(std::is_assignable_v<TestType, short>);
  STATIC_REQUIRE(std::is_assignable_v<TestType, signed short>);
  STATIC_REQUIRE(std::is_assignable_v<TestType, unsigned short>);
  STATIC_REQUIRE(std::is_assignable_v<TestType, int>);
  STATIC_REQUIRE(std::is_assignable_v<TestType, signed int>);
  STATIC_REQUIRE(std::is_assignable_v<TestType, unsigned int>);
  STATIC_REQUIRE(std::is_assignable_v<TestType, long>);
  STATIC_REQUIRE(std::is_assignable_v<TestType, signed long>);
  STATIC_REQUIRE(std::is_assignable_v<TestType, unsigned long>);
  STATIC_REQUIRE(std::is_assignable_v<TestType, long long>);
  STATIC_REQUIRE(std::is_assignable_v<TestType, signed long long>);
  STATIC_REQUIRE(std::is_assignable_v<TestType, unsigned long long>);
}

TEMPLATE_TEST_CASE("A fixed-width counter is assignable with an integer", "", champsim::msl::fwcounter<2>, champsim::msl::sfwcounter<2>) {
  TestType lhs{1};
  lhs = 0;
  REQUIRE(lhs.value() == 0);
}

TEMPLATE_TEST_CASE("A fixed-width counter is assignable with an out-of-bounds integer", "", champsim::msl::fwcounter<2>, champsim::msl::sfwcounter<2>) {
  TestType lhs{1};
  lhs = 100;
  REQUIRE(lhs.value() == lhs.maximum);
}

