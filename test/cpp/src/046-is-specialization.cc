#include <catch.hpp>

#include "util/type_traits.h"

#include <list>
#include <vector>

struct A {};

TEST_CASE("is_specialization recognizes non-templates") {
  REQUIRE_FALSE(champsim::is_specialization_v<A, std::vector>);
  REQUIRE_FALSE(champsim::is_specialization<A, std::vector>::value);
}

TEST_CASE("is_specialization recognizes templates") {
  REQUIRE(champsim::is_specialization_v<std::vector<A>, std::vector>);
  REQUIRE(champsim::is_specialization<std::vector<A>, std::vector>::value);
}

TEST_CASE("is_specialization recognizes specializations of other templates") {
  REQUIRE_FALSE(champsim::is_specialization_v<std::list<A>, std::vector>);
  REQUIRE_FALSE(champsim::is_specialization<std::list<A>, std::vector>::value);
}
