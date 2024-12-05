#include <catch.hpp>
#include <string>
#include <type_traits>

#include "waitable.h"

using time_point = champsim::chrono::clock::time_point;
using duration = typename time_point::duration;

TEST_CASE("An empty waitable has indeterminate readiness", "") {
  champsim::waitable<int> seed{};
  REQUIRE(seed.has_unknown_readiness());
}

TEST_CASE("A waitable maps its value", "") {
  champsim::waitable<int> seed{40};
  auto result = seed.map([](int i) { return i + 2; });
  STATIC_REQUIRE(std::is_same_v<decltype(result), champsim::waitable<int>>);
  REQUIRE(result.value() == 42);
}

TEST_CASE("An indeterminate waitable is indeterminate after mapping", "") {
  champsim::waitable<int> seed{};
  auto result = seed.map([](int i) { return i + 2; });
  STATIC_REQUIRE(std::is_same_v<decltype(result), champsim::waitable<int>>);
  REQUIRE(result.has_unknown_readiness());
}

TEST_CASE("An unready waitable is unready after mapping", "") {
  champsim::waitable<int> seed{40, time_point{duration{5}}};
  auto result = seed.map([](int i) { return i + 2; });
  STATIC_REQUIRE(std::is_same_v<decltype(result), champsim::waitable<int>>);
  REQUIRE_FALSE(result.is_ready_at(time_point{duration{4}}));
}

TEST_CASE("A ready waitable is ready after mapping", "") {
  champsim::waitable<int> seed{40, time_point{duration{3}}};
  auto result = seed.map([](int i) { return i + 2; });
  STATIC_REQUIRE(std::is_same_v<decltype(result), champsim::waitable<int>>);
  REQUIRE(result.is_ready_at(time_point{duration{4}}));
}

struct rval_call_map {
  double operator()(int) && { return 42.0; }
};

TEST_CASE("A waitable rvalue-maps its value", "") {
  champsim::waitable<int> seed{42};
  auto result = seed.map(rval_call_map{});
  STATIC_REQUIRE(std::is_same_v<decltype(result), champsim::waitable<double>>);
  REQUIRE(result.value() == 42.0);
}

TEST_CASE("An rvalue waitable maps its value", "") {
  champsim::waitable<int> seed{40};
  auto result = std::move(seed).map([](int &&i) { return i + 2; });
  STATIC_REQUIRE(std::is_same_v<decltype(result), champsim::waitable<int>>);
  REQUIRE(result.value() == 42);
}

TEST_CASE("A const waitable maps its value", "") {
  champsim::waitable<int> seed{40};
  auto result = seed.map([](const int &i) { return i + 2; });
  STATIC_REQUIRE(std::is_same_v<decltype(result), champsim::waitable<int>>);
  REQUIRE(result.value() == 42);
}

TEST_CASE("A waitable fmaps its value", "") {
  champsim::waitable<int> seed{12};
  auto result = seed.and_then([](int) { return champsim::waitable<float>{42.f}; });
  STATIC_REQUIRE(std::is_same_v<decltype(result), champsim::waitable<float>>);
  REQUIRE(result.value() == 42.f);
}

TEST_CASE("An indeterminate waitable can be ready after fmapping", "") {
  champsim::waitable<int> seed{40};
  auto result = seed.and_then([](int) { return champsim::waitable<float>{42.f, time_point{duration{1}}}; });
  STATIC_REQUIRE(std::is_same_v<decltype(result), champsim::waitable<float>>);
  REQUIRE(result.is_ready_at(time_point{duration{4}}));
}

TEST_CASE("An unready waitable can be ready after fmapping", "") {
  champsim::waitable<int> seed{40, time_point{duration{5}}};
  auto result = seed.and_then([](int) { return champsim::waitable<float>{42.f, time_point{duration{1}}}; });
  STATIC_REQUIRE(std::is_same_v<decltype(result), champsim::waitable<float>>);
  REQUIRE(result.is_ready_at(time_point{duration{4}}));
}

TEST_CASE("A ready waitable is unready after fmapping", "") {
  champsim::waitable<int> seed{40, time_point{duration{3}}};
  auto result = seed.and_then([](int) { return champsim::waitable<float>{42.f, time_point{duration{5}}}; });
  STATIC_REQUIRE(std::is_same_v<decltype(result), champsim::waitable<float>>);
  REQUIRE_FALSE(result.is_ready_at(time_point{duration{4}}));
}

struct rval_call_and_then {
  champsim::waitable<double> operator()(int) && { return champsim::waitable{42.0}; }
};

TEST_CASE("A waitable rvalue-fmaps its value", "") {
  champsim::waitable<int> seed{42};
  auto result = seed.and_then(rval_call_and_then{});
  STATIC_REQUIRE(std::is_same_v<decltype(result), champsim::waitable<double>>);
  REQUIRE(result.value() == 42.0);
}

TEST_CASE("An rvalue waitable fmaps its value", "") {
  champsim::waitable<int> seed{42};
  auto result = std::move(seed).and_then([](int &&i) { return champsim::waitable<double>(i); });
  STATIC_REQUIRE(std::is_same_v<decltype(result), champsim::waitable<double>>);
  REQUIRE(result.value() == 42.0);
}

TEST_CASE("A const waitable fmaps its value", "") {
  champsim::waitable<int> seed{42};
  auto result = seed.and_then([](const int &i) { return champsim::waitable<double>(i); });
  STATIC_REQUIRE(std::is_same_v<decltype(result), champsim::waitable<double>>);
  REQUIRE(result.value() == 42.0);
}
