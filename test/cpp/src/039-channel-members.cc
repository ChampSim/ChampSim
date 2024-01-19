#include <catch.hpp>
#include "channel.h"

#include <type_traits>

TEST_CASE("Channel member types are copy constructible") {
  STATIC_REQUIRE(std::is_copy_constructible_v<champsim::channel::request_type>);
  STATIC_REQUIRE(std::is_copy_constructible_v<champsim::channel::response_type>);
}

TEST_CASE("Channel member types are copy assignable") {
  STATIC_REQUIRE(std::is_copy_assignable_v<champsim::channel::request_type>);
  STATIC_REQUIRE(std::is_copy_assignable_v<champsim::channel::response_type>);
}

TEST_CASE("Channel member types are move constructible") {
  STATIC_REQUIRE(std::is_move_constructible_v<champsim::channel::request_type>);
  STATIC_REQUIRE(std::is_move_constructible_v<champsim::channel::response_type>);
}

TEST_CASE("Channel member types are move assignable") {
  STATIC_REQUIRE(std::is_move_assignable_v<champsim::channel::request_type>);
  STATIC_REQUIRE(std::is_move_assignable_v<champsim::channel::response_type>);
}

TEST_CASE("Channel member types are destructible") {
  STATIC_REQUIRE(std::is_destructible_v<champsim::channel::request_type>);
  STATIC_REQUIRE(std::is_destructible_v<champsim::channel::response_type>);
}

TEST_CASE("Channel member types are swappable") {
  STATIC_REQUIRE(std::is_swappable_v<champsim::channel::request_type>);
  STATIC_REQUIRE(std::is_swappable_v<champsim::channel::response_type>);
}

