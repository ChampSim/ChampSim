#include "catch.hpp"

#include "champsim.h"
#include "address.h"

#include <iomanip>
#include <sstream>
#include <fmt/core.h>

#include "util/detect.h"
#include "champsim_constants.h"

TEST_CASE("An address is constructible from a uint64_t") {
  STATIC_REQUIRE(std::is_constructible_v<champsim::address, uint64_t>);

  auto address = GENERATE(as<uint64_t>{}, 0xffff'ffff'ffff'ffff, 0x8000'0000'0000'0000);

  champsim::address test_a{address};
  REQUIRE(test_a.to<uint64_t>() == address);
}

TEST_CASE("An address is copy constructible") {
  STATIC_REQUIRE(std::is_copy_constructible_v<champsim::address>);

  champsim::address control{0xffff'ffff'ffff'ffff};
  champsim::address test_a{0xffff'ffff'ffff'ffff};
  REQUIRE(test_a == control);

  champsim::address test_b{test_a};
  REQUIRE(test_a == test_b);
}

TEST_CASE("An address is move constructible") {
  STATIC_REQUIRE(std::is_move_constructible_v<champsim::address>);

  champsim::address control{0xffff'ffff'ffff'ffff};
  champsim::address test_a{0xffff'ffff'ffff'ffff};
  REQUIRE(test_a == control);

  champsim::address test_b{std::move(test_a)};
  REQUIRE(test_b == control);
}

TEST_CASE("An address is copy assignable") {
  STATIC_REQUIRE(std::is_copy_assignable_v<champsim::address>);

  champsim::address test_a{};
  champsim::address test_b{0xffff'ffff'ffff'ffff};
  champsim::address control{0xffff'ffff'ffff'ffff};
  REQUIRE(test_b == control);
  REQUIRE(test_a != test_b);

  test_a = test_b;
  REQUIRE(test_a == test_b);
  REQUIRE(test_a == control);
  REQUIRE(test_b == control);
}

TEST_CASE("An address is move assignable") {
  STATIC_REQUIRE(std::is_move_assignable_v<champsim::address>);

  champsim::address control{0xffff'ffff'ffff'ffff};
  champsim::address test_a{0xffff'ffff'ffff'ffff};
  champsim::address test_b{};
  REQUIRE(test_a == control);
  REQUIRE(test_b != control);

  test_b = std::move(test_a);
  REQUIRE(test_b == control);
}

TEST_CASE("An address is swappable") {
  STATIC_REQUIRE(std::is_swappable_v<champsim::address>);

  champsim::address control_a{0xffff'ffff'ffff'ffff};
  champsim::address test_a{0xffff'ffff'ffff'ffff};
  champsim::address control_b{0xcafebabe};
  champsim::address test_b{0xcafebabe};

  REQUIRE(test_a == control_a);
  REQUIRE(test_b == control_b);

  std::swap(test_a, test_b);

  REQUIRE(test_b == control_a);
  REQUIRE(test_a == control_b);
}

TEST_CASE("An address compares for equality") {
  champsim::address lhs{10};
  REQUIRE_FALSE(lhs == champsim::address{9});
  REQUIRE(lhs == champsim::address{10});
  REQUIRE_FALSE(lhs == champsim::address{11});
}

TEST_CASE("An address compares for inequality") {
  champsim::address lhs{10};
  REQUIRE(lhs != champsim::address{9});
  REQUIRE_FALSE(lhs != champsim::address{10});
  REQUIRE(lhs != champsim::address{11});
}

TEST_CASE("An address compares less") {
  champsim::address lhs{10};
  REQUIRE_FALSE(lhs < champsim::address{9});
  REQUIRE_FALSE(lhs < champsim::address{10});
  REQUIRE(lhs < champsim::address{11});
}

TEST_CASE("An address compares less than or equal") {
  champsim::address lhs{10};
  REQUIRE_FALSE(lhs <= champsim::address{9});
  REQUIRE(lhs <= champsim::address{10});
  REQUIRE(lhs <= champsim::address{11});
}

TEST_CASE("An address compares greater") {
  champsim::address lhs{10};
  REQUIRE_FALSE(lhs < champsim::address{9});
  REQUIRE_FALSE(lhs < champsim::address{10});
  REQUIRE(lhs < champsim::address{11});
}

TEST_CASE("An address compares greater than or equal") {
  champsim::address lhs{10};
  REQUIRE(lhs >= champsim::address{9});
  REQUIRE(lhs >= champsim::address{10});
  REQUIRE_FALSE(lhs >= champsim::address{11});
}

TEST_CASE("An address can add") {
  champsim::address lhs{1};
  auto result = lhs + 1;
  REQUIRE(result == champsim::address{2});
}

TEST_CASE("An address can add in place") {
  champsim::address lhs{1};
  lhs += 1;
  REQUIRE(lhs == champsim::address{2});
}

TEST_CASE("An address can subtract") {
  champsim::address lhs{3};
  auto result = lhs - 1;
  REQUIRE(result == champsim::address{2});
}

TEST_CASE("An address can subtract in place") {
  champsim::address lhs{3};
  lhs -= 1;
  REQUIRE(lhs == champsim::address{2});
}

TEST_CASE("An address can add a negative number") {
  champsim::address lhs{3};
  auto result = lhs + -1;
  REQUIRE(result == champsim::address{2});
}

TEST_CASE("An address can add a negative number in place") {
  champsim::address lhs{3};
  lhs += -1;
  REQUIRE(lhs == champsim::address{2});
}

TEST_CASE("An address can subtract a negative number") {
  champsim::address lhs{1};
  auto result = lhs - (-1);
  REQUIRE(result == champsim::address{2});
}

TEST_CASE("An address can subtract a negative number in place") {
  champsim::address lhs{1};
  lhs -= -1;
  REQUIRE(lhs == champsim::address{2});
}

/*
TEST_CASE("An address can be shown to be a block address") {
  CHECK(champsim::address{0xffff'ffff & ~champsim::bitmask(LOG2_BLOCK_SIZE + 4)}.is_block_address());
  CHECK(champsim::address{0xffff'ffff & ~champsim::bitmask(LOG2_BLOCK_SIZE + 2)}.is_block_address());
  CHECK(champsim::address{0xffff'ffff & ~champsim::bitmask(LOG2_BLOCK_SIZE + 1)}.is_block_address());
  CHECK(champsim::address{0xffff'ffff & ~champsim::bitmask(LOG2_BLOCK_SIZE)}.is_block_address());
  CHECK_FALSE(champsim::address{0xffff'ffff & ~champsim::bitmask(LOG2_BLOCK_SIZE - 1)}.is_block_address());
  CHECK_FALSE(champsim::address{0xffff'ffff & ~champsim::bitmask(LOG2_BLOCK_SIZE - 2)}.is_block_address());
  CHECK_FALSE(champsim::address{0xffff'ffff & ~champsim::bitmask(LOG2_BLOCK_SIZE - 4)}.is_block_address());
  CHECK_FALSE(champsim::address{0xffff'ffff}.is_block_address());
}

TEST_CASE("An address can be shown to be a page address") {
  CHECK(champsim::address{0xffff'ffff & ~champsim::bitmask(LOG2_PAGE_SIZE + 4)}.is_page_address());
  CHECK(champsim::address{0xffff'ffff & ~champsim::bitmask(LOG2_PAGE_SIZE + 3)}.is_page_address());
  CHECK(champsim::address{0xffff'ffff & ~champsim::bitmask(LOG2_PAGE_SIZE + 2)}.is_page_address());
  CHECK(champsim::address{0xffff'ffff & ~champsim::bitmask(LOG2_PAGE_SIZE + 1)}.is_page_address());
  CHECK(champsim::address{0xffff'ffff & ~champsim::bitmask(LOG2_PAGE_SIZE)}.is_page_address());
  CHECK_FALSE(champsim::address{0xffff'ffff & ~champsim::bitmask(LOG2_PAGE_SIZE - 1)}.is_page_address());
  CHECK_FALSE(champsim::address{0xffff'ffff & ~champsim::bitmask(LOG2_PAGE_SIZE - 2)}.is_page_address());
  CHECK_FALSE(champsim::address{0xffff'ffff & ~champsim::bitmask(LOG2_PAGE_SIZE - 3)}.is_page_address());
  CHECK_FALSE(champsim::address{0xffff'ffff & ~champsim::bitmask(LOG2_PAGE_SIZE - 4)}.is_page_address());
  CHECK_FALSE(champsim::address{0xffff'ffff}.is_page_address());
}
*/

namespace {
  template <typename SliceA, typename SliceB>
    using can_find_offset = decltype( champsim::offset(std::declval<SliceA>(), std::declval<SliceB>()) );
}

TEST_CASE("The offset between two addresses is signed") {
  STATIC_REQUIRE(champsim::is_detected_v<::can_find_offset, champsim::address, champsim::address>);
  STATIC_REQUIRE(std::is_signed_v<::can_find_offset<champsim::address, champsim::address>>);
  STATIC_REQUIRE(champsim::is_detected_v<::can_find_offset, champsim::block_number, champsim::block_number>);
  STATIC_REQUIRE(std::is_signed_v<::can_find_offset<champsim::block_number, champsim::block_number>>);
}

TEST_CASE("The offset between two addresses is correct") {
  // Small differences
  CHECK(champsim::offset(champsim::address{0xffff'ffff}, champsim::address{0xffff'ffff}) == 0);
  CHECK(champsim::offset(champsim::address{0xffff'ffff}, champsim::address{0xffff'fffe}) == -1);
  CHECK(champsim::offset(champsim::address{0xffff'ffff}, champsim::address{0xffff'fff0}) == -15);
  CHECK(champsim::offset(champsim::address{0xffff'fffe}, champsim::address{0xffff'ffff}) == 1);
  CHECK(champsim::offset(champsim::address{0xffff'fff0}, champsim::address{0xffff'ffff}) == 15);

  // Large differences
  CHECK(champsim::offset(champsim::address{0x8000'0000'0000'0000}, champsim::address{0xffff'ffff'ffff'ffff}) == std::numeric_limits<champsim::address::difference_type>::max());
  CHECK(champsim::offset(champsim::address{0xffff'ffff'ffff'ffff}, champsim::address{0x8000'0000'0000'0000}) == std::numeric_limits<champsim::address::difference_type>::min()+1);
  CHECK(champsim::offset(champsim::address{0x0000'0000'0000'0000}, champsim::address{0x7fff'ffff'ffff'ffff}) == std::numeric_limits<champsim::address::difference_type>::max());
  CHECK(champsim::offset(champsim::address{0x7fff'ffff'ffff'ffff}, champsim::address{0x0000'0000'0000'0000}) == std::numeric_limits<champsim::address::difference_type>::min()+1);

  CHECK_THROWS(champsim::offset(champsim::address{0x8000'0000'0000'0000}, champsim::address{0x0000'0000'0000'0000}));
}

TEST_CASE("An address prints something at all") {
  std::ostringstream strstr;
  champsim::address addr{0xffff'ffff};
  strstr << addr;
  CHECK(strstr.str().size() > 0);
  CHECK(strstr.str() == "0xffffffff");
}

TEST_CASE("std::setw affects the width of the printed address") {
  std::ostringstream strstr;
  champsim::address addr{0xffff'ffff};
  strstr << std::setw(18) << addr;
  REQUIRE(strstr.str() == "0x00000000ffffffff");
}

TEST_CASE("An address prints something to libfmt") {
  champsim::address addr{0xffff'ffff};
  auto result = fmt::format("{}", addr);
  REQUIRE_THAT(result, Catch::Matchers::Matches("0xffffffff"));
}

TEST_CASE("The libfmt specifiers affect the width of the address") {
  champsim::address addr{0xffff'ffff};
  auto result = fmt::format("{:18}", addr);
  REQUIRE_THAT(result, Catch::Matchers::Matches( "0x00000000ffffffff"));
}
