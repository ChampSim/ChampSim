#include <catch.hpp>
#include "util/span.h"

#include <vector>

TEST_CASE("get_span() returns an empty span from an empty list") {
  auto size = GENERATE(0, 1, 2, 4, 10, 20, 100, 400);
  std::vector<int> test_vec{};
  auto [begin, end] = champsim::get_span(std::begin(test_vec), std::end(test_vec), size);
  REQUIRE(std::distance(begin, end) == 0);
}

TEST_CASE("get_span() returns a span capped by the size parameter") {
  auto size = GENERATE(0, 1, 2, 4, 10, 20, 100);
  std::vector<int> test_vec(400, -1);
  auto [begin, end] = champsim::get_span(std::begin(test_vec), std::end(test_vec), size);
  REQUIRE(std::distance(begin, end) == size);
}

TEST_CASE("get_span_p() returns an empty span from an empty list") {
  auto size = GENERATE(0, 1, 2, 4, 10, 20, 100, 400);
  std::vector<int> test_vec{};
  auto [begin, end] = champsim::get_span_p(std::begin(test_vec), std::end(test_vec), size, [](auto){ return true; });
  REQUIRE(std::distance(begin, end) == 0);
}

TEST_CASE("get_span_p() returns a span capped by the size parameter") {
  auto size = GENERATE(0, 1, 2, 4, 10, 20, 100);
  std::vector<int> test_vec(400, -1);
  auto [begin, end] = champsim::get_span_p(std::begin(test_vec), std::end(test_vec), size, [](auto){ return true; });
  REQUIRE(std::distance(begin, end) == size);
}

TEST_CASE("get_span_p() returns a span capped by the function parameter") {
  std::vector<int> test_vec(400, -1);
  test_vec.at(10) = 1;
  auto [begin, end] = champsim::get_span_p(std::begin(test_vec), std::end(test_vec), 400, [](auto x){ return x < 0; });
  REQUIRE(std::distance(begin, end) == 10);
}

