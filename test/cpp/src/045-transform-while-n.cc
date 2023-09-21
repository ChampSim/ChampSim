#include <catch.hpp>

#include "util/algorithm.h"

#include <vector>

namespace {
template <typename T>
bool always(T /*ignored*/)
{
  return true;
}

template <typename T>
bool is_negative(T val)
{
  return val < 0;
}

template <typename T>
T identity(T val)
{
  return val;
}

template <typename T>
T negative(T val)
{
  return -1*val;
}

}

TEST_CASE("transform_while_n() does not transform an empty list") {
  std::vector<int> source{};
  std::vector<int> destination{};

  auto sz = champsim::transform_while_n(source, std::back_inserter(destination), champsim::bandwidth{champsim::bandwidth::maximum_type{4000}}, ::always<int>, ::identity<int>);

  REQUIRE_THAT(destination, Catch::Matchers::IsEmpty());
  REQUIRE(sz == 0);
}

TEST_CASE("transform_while_n() is capped by the size parameter") {
  auto size = GENERATE(as<champsim::bandwidth::maximum_type>{}, 0, 1, 2, 4, 10, 20, 100);
  std::vector<int> source(400, -1);
  std::vector<int> destination{};

  auto sz = champsim::transform_while_n(source, std::back_inserter(destination), champsim::bandwidth{size}, ::always<int>, ::negative<int>);

  REQUIRE_THAT(destination, Catch::Matchers::RangeEquals(std::vector<int>((std::size_t)size, 1)));
  REQUIRE(sz == (long)size);
}

TEST_CASE("transform_while_n() is capped by the function parameter") {
  std::vector<int> source(400, -1);
  std::vector<int> destination{};
  source.at(10) = 1;

  auto sz = champsim::transform_while_n(source, std::back_inserter(destination), champsim::bandwidth{champsim::bandwidth::maximum_type{4000}}, ::is_negative<int>, ::negative<int>);

  REQUIRE_THAT(destination, Catch::Matchers::RangeEquals(std::vector<int>(10, 1)));
  REQUIRE(sz == 10);
}
