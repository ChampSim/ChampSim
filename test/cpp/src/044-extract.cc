#include <catch.hpp>
#include "util/algorithm.h"

#include <vector>

TEST_CASE("extract_if() does nothing on an empty list") {
  std::vector<int> in_vec{};
  std::vector<int> out_vec{};
  champsim::extract_if(std::begin(in_vec), std::end(in_vec), std::back_inserter(out_vec), [](auto x){ return x % 2 == 0; });
  CHECK_THAT(out_vec, Catch::Matchers::IsEmpty());
}

TEST_CASE("extract_if() can move an none of a list") {
  std::vector<int> in_vec(400, 1);
  std::vector<int> out_vec{};
  auto is_even = [](auto x){ return x % 2 == 0; };
  auto [in_end, out_end] = champsim::extract_if(std::begin(in_vec), std::end(in_vec), std::back_inserter(out_vec), is_even);
  in_vec.erase(in_end, std::end(in_vec));
  CHECK_THAT(in_vec, Catch::Matchers::SizeIs(400) && Catch::Matchers::AllMatch(Catch::Matchers::Predicate<int>(std::not_fn(is_even), "is odd")));
  CHECK_THAT(out_vec, Catch::Matchers::IsEmpty());
}

TEST_CASE("extract_if() can move an entire list") {
  std::vector<int> in_vec(400, 2);
  std::vector<int> out_vec{};
  auto is_even = [](auto x){ return x % 2 == 0; };
  auto [in_end, out_end] = champsim::extract_if(std::begin(in_vec), std::end(in_vec), std::back_inserter(out_vec), is_even);
  in_vec.erase(in_end, std::end(in_vec));
  CHECK_THAT(in_vec, Catch::Matchers::IsEmpty());
  CHECK_THAT(out_vec, Catch::Matchers::SizeIs(400) && Catch::Matchers::AllMatch(Catch::Matchers::Predicate<int>(is_even, "is even")));
}

TEST_CASE("extract_if() can split a mixed list") {
  std::vector<int> in_vec(400, 0);
  std::vector<int> out_vec{};
  std::iota(std::begin(in_vec), std::end(in_vec), 0);
  auto is_even = [](auto x){ return x % 2 == 0; };
  auto [in_end, out_end] = champsim::extract_if(std::begin(in_vec), std::end(in_vec), std::back_inserter(out_vec), is_even);
  in_vec.erase(in_end, std::end(in_vec));
  CHECK_THAT(in_vec, Catch::Matchers::SizeIs(200) && Catch::Matchers::AllMatch(Catch::Matchers::Predicate<int>(std::not_fn(is_even), "is odd")));
  CHECK_THAT(out_vec, Catch::Matchers::SizeIs(200) && Catch::Matchers::AllMatch(Catch::Matchers::Predicate<int>(is_even, "is even")));
}

