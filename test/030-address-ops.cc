#include "catch.hpp"

#include "address.h"
#include "champsim_constants.h"

TEST_CASE("An address is constructible by certian means") {
  STATIC_REQUIRE(std::is_constructible_v<champsim::address, uint64_t>);
  STATIC_REQUIRE(std::is_copy_constructible_v<champsim::address>);
  STATIC_REQUIRE(std::is_move_constructible_v<champsim::address>);
  STATIC_REQUIRE(std::is_copy_assignable_v<champsim::address>);
  STATIC_REQUIRE(std::is_move_assignable_v<champsim::address>);
  STATIC_REQUIRE(std::is_destructible_v<champsim::address>);
  STATIC_REQUIRE(std::is_swappable_v<champsim::address>);
  STATIC_REQUIRE(std::is_assignable_v<champsim::address, champsim::address>);
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

TEST_CASE("The offset between two addresses is signed") {
  STATIC_REQUIRE(std::is_signed_v<std::invoke_result_t<decltype(&champsim::address::offset), champsim::address, champsim::address>>);
}

TEST_CASE("The offset between two addresses is correct") {
  // Small differences
  CHECK(champsim::address::offset(champsim::address{0xffff'ffff}, champsim::address{0xffff'ffff}) == 0);
  CHECK(champsim::address::offset(champsim::address{0xffff'ffff}, champsim::address{0xffff'fffe}) == -1);
  CHECK(champsim::address::offset(champsim::address{0xffff'ffff}, champsim::address{0xffff'fff0}) == -15);
  CHECK(champsim::address::offset(champsim::address{0xffff'fffe}, champsim::address{0xffff'ffff}) == 1);
  CHECK(champsim::address::offset(champsim::address{0xffff'fff0}, champsim::address{0xffff'ffff}) == 15);

  // Large differences
  CHECK(champsim::address::offset(champsim::address{0x8000'0000'0000'0000}, champsim::address{0xffff'ffff'ffff'ffff}) == std::numeric_limits<champsim::address::difference_type>::max());
  CHECK(champsim::address::offset(champsim::address{0xffff'ffff'ffff'ffff}, champsim::address{0x8000'0000'0000'0000}) == std::numeric_limits<champsim::address::difference_type>::min()+1);
  CHECK(champsim::address::offset(champsim::address{0x0000'0000'0000'0000}, champsim::address{0x7fff'ffff'ffff'ffff}) == std::numeric_limits<champsim::address::difference_type>::max());
  CHECK(champsim::address::offset(champsim::address{0x7fff'ffff'ffff'ffff}, champsim::address{0x0000'0000'0000'0000}) == std::numeric_limits<champsim::address::difference_type>::min()+1);
}
