#define CATCH_CONFIG_RUNTIME_STATIC_REQUIRE
#include <catch.hpp>

#include "extent.h"
#include "util/units.h"

using namespace champsim::data::data_literals;

TEMPLATE_TEST_CASE_SIG("The union of adjacent static extents is bounded by the maximum and minimum", "", ((champsim::data::bits V), V), 4_b, 8_b, 12_b, 16_b, 20_b, 24_b, 28_b) {
  champsim::static_extent<32_b,V> lhs{};
  champsim::static_extent<V,0_b> rhs{};

  REQUIRE(champsim::extent_union(lhs, rhs).upper == 32_b);
  REQUIRE(champsim::extent_union(lhs, rhs).lower == 0_b);
}

TEMPLATE_TEST_CASE_SIG("Static extents that are subsets can be unioned", "", ((champsim::data::bits V, champsim::data::bits W), V, W), (8_b,4_b), (12_b,4_b), (16_b,4_b), (20_b,4_b), (24_b,4_b), (28_b,4_b), (12_b,8_b), (16_b,8_b), (20_b,8_b), (24_b,8_b), (28_b,8_b), (16_b,12_b), (20_b,12_b), (24_b,12_b), (28_b,12_b), (20_b,16_b), (24_b,16_b), (28_b,16_b),  (24_b,20_b), (28_b,20_b), (28_b,24_b)) {
  champsim::static_extent<32_b,0_b> lhs{};
  champsim::static_extent<V,W> rhs{};

  REQUIRE(champsim::extent_union(lhs, rhs).upper == 32_b);
  REQUIRE(champsim::extent_union(lhs, rhs).lower == 0_b);
}

TEST_CASE("The union of adjacent dynamic extents is bounded by the maximum and minimum") {
  auto v = GENERATE(4_b, 8_b, 12_b, 16_b, 20_b, 24_b, 28_b);
  champsim::dynamic_extent lhs{32_b, v};
  champsim::dynamic_extent rhs{v, 0_b};

  REQUIRE(champsim::extent_union(lhs, rhs).upper == 32_b);
  REQUIRE(champsim::extent_union(lhs, rhs).lower == 0_b);
}

TEST_CASE("Dynamic extents that are subsets can be unioned") {
  auto [v,w] = GENERATE(as<std::pair<champsim::data::bits, champsim::data::bits>>{},
      std::pair{8_b,4_b}, std::pair{12_b,4_b}, std::pair{16_b,4_b}, std::pair{20_b,4_b}, std::pair{24_b,4_b}, std::pair{28_b,4_b},
      std::pair{12_b,8_b}, std::pair{16_b,8_b}, std::pair{20_b,8_b}, std::pair{24_b,8_b}, std::pair{28_b,8_b},
      std::pair{16_b,12_b}, std::pair{20_b,12_b}, std::pair{24_b,12_b}, std::pair{28_b,12_b},
      std::pair{20_b,16_b}, std::pair{24_b,16_b}, std::pair{28_b,16_b},
      std::pair{24_b,20_b}, std::pair{28_b,20_b},
      std::pair{28_b,24_b});

  champsim::dynamic_extent lhs{32_b,0_b};
  champsim::dynamic_extent rhs{v,w};

  REQUIRE(champsim::extent_union(lhs, rhs).upper == 32_b);
  REQUIRE(champsim::extent_union(lhs, rhs).lower == 0_b);
}

TEMPLATE_TEST_CASE_SIG("The union of adjacent mismatched extents is bounded by the maximum and minimum", "", ((champsim::data::bits V), V), 4_b, 8_b, 12_b, 16_b, 20_b, 24_b, 28_b) {
  champsim::static_extent<32_b,V> lhs{};
  champsim::dynamic_extent rhs{V,0_b};

  REQUIRE(champsim::extent_union(lhs, rhs).upper == 32_b);
  REQUIRE(champsim::extent_union(lhs, rhs).lower == 0_b);
  REQUIRE(champsim::extent_union(rhs, lhs).upper == 32_b);
  REQUIRE(champsim::extent_union(rhs, lhs).lower == 0_b);
}

TEST_CASE("Mismatched extents that are subsets can be unioned") {
  auto [v,w] = GENERATE(as<std::pair<champsim::data::bits, champsim::data::bits>>{},
      std::pair{8_b,4_b}, std::pair{12_b,4_b}, std::pair{16_b,4_b}, std::pair{20_b,4_b}, std::pair{24_b,4_b}, std::pair{28_b,4_b},
      std::pair{12_b,8_b}, std::pair{16_b,8_b}, std::pair{20_b,8_b}, std::pair{24_b,8_b}, std::pair{28_b,8_b},
      std::pair{16_b,12_b}, std::pair{20_b,12_b}, std::pair{24_b,12_b}, std::pair{28_b,12_b},
      std::pair{20_b,16_b}, std::pair{24_b,16_b}, std::pair{28_b,16_b},
      std::pair{24_b,20_b}, std::pair{28_b,20_b},
      std::pair{28_b,24_b});

  champsim::static_extent<32_b,0_b> lhs{};
  champsim::dynamic_extent rhs{v,w};

  REQUIRE(champsim::extent_union(lhs, rhs).upper == 32_b);
  REQUIRE(champsim::extent_union(lhs, rhs).lower == 0_b);
  REQUIRE(champsim::extent_union(rhs, lhs).upper == 32_b);
  REQUIRE(champsim::extent_union(rhs, lhs).lower == 0_b);
}

TEMPLATE_TEST_CASE_SIG("Static extents can be subextented", "", ((champsim::data::bits V, champsim::data::bits W), V, W), (8_b,4_b), (12_b,4_b), (16_b,4_b), (20_b,4_b), (24_b,4_b), (28_b,4_b), (12_b,8_b), (16_b,8_b), (20_b,8_b), (24_b,8_b), (28_b,8_b), (16_b,12_b), (20_b,12_b), (24_b,12_b), (28_b,12_b), (20_b,16_b), (24_b,16_b), (28_b,16_b),  (24_b,20_b), (28_b,20_b), (28_b,24_b)) {
  {
    champsim::static_extent<32_b,0_b> lhs{};
    champsim::static_extent<V,W> rhs{};

    REQUIRE(champsim::relative_extent(lhs, rhs).upper == V);
    REQUIRE(champsim::relative_extent(lhs, rhs).lower == W);
  }

  {
    champsim::static_extent<36_b,4_b> lhs{};
    champsim::static_extent<V,W> rhs{};

    REQUIRE(champsim::relative_extent(lhs, rhs).upper == V+4_b);
    REQUIRE(champsim::relative_extent(lhs, rhs).lower == W+4_b);
  }
}

TEMPLATE_TEST_CASE_SIG("Static extents do not overflow when subextented", "", ((champsim::data::bits V, champsim::data::bits W), V, W), (8_b,4_b), (12_b,4_b), (16_b,4_b), (20_b,4_b), (24_b,4_b), (28_b,4_b), (12_b,8_b), (16_b,8_b), (20_b,8_b), (24_b,8_b), (28_b,8_b), (16_b,12_b), (20_b,12_b), (24_b,12_b), (28_b,12_b), (20_b,16_b), (24_b,16_b), (28_b,16_b),  (24_b,20_b), (28_b,20_b), (28_b,24_b)) {
  champsim::static_extent<16_b,8_b> lhs{};
  champsim::static_extent<V,W> rhs{};

  REQUIRE(champsim::relative_extent(lhs, rhs).upper == std::min(V,8_b)+8_b);
  REQUIRE(champsim::relative_extent(lhs, rhs).lower == std::min(W,8_b)+8_b);
}

TEST_CASE("Dynamic extents can be subextented") {
  auto [v,w] = GENERATE(as<std::pair<champsim::data::bits, champsim::data::bits>>{},
      std::pair{8_b,4_b}, std::pair{12_b,4_b}, std::pair{16_b,4_b}, std::pair{20_b,4_b}, std::pair{24_b,4_b}, std::pair{28_b,4_b},
      std::pair{12_b,8_b}, std::pair{16_b,8_b}, std::pair{20_b,8_b}, std::pair{24_b,8_b}, std::pair{28_b,8_b},
      std::pair{16_b,12_b}, std::pair{20_b,12_b}, std::pair{24_b,12_b}, std::pair{28_b,12_b},
      std::pair{20_b,16_b}, std::pair{24_b,16_b}, std::pair{28_b,16_b},
      std::pair{24_b,20_b}, std::pair{28_b,20_b},
      std::pair{28_b,24_b});

  {
    champsim::dynamic_extent lhs{32_b,0_b};
    champsim::dynamic_extent rhs{v,w};

    REQUIRE(champsim::relative_extent(lhs, rhs).upper == v);
    REQUIRE(champsim::relative_extent(lhs, rhs).lower == w);
  }

  {
    champsim::dynamic_extent lhs{36_b,4_b};
    champsim::dynamic_extent rhs{v,w};

    REQUIRE(champsim::relative_extent(lhs, rhs).upper == v+4_b);
    REQUIRE(champsim::relative_extent(lhs, rhs).lower == w+4_b);
  }
}

TEST_CASE("Dynamic extents do not overflow when subextented") {
  auto [v,w] = GENERATE(as<std::pair<champsim::data::bits, champsim::data::bits>>{},
      std::pair{8_b,4_b}, std::pair{12_b,4_b}, std::pair{16_b,4_b}, std::pair{20_b,4_b}, std::pair{24_b,4_b}, std::pair{28_b,4_b},
      std::pair{12_b,8_b}, std::pair{16_b,8_b}, std::pair{20_b,8_b}, std::pair{24_b,8_b}, std::pair{28_b,8_b},
      std::pair{16_b,12_b}, std::pair{20_b,12_b}, std::pair{24_b,12_b}, std::pair{28_b,12_b},
      std::pair{20_b,16_b}, std::pair{24_b,16_b}, std::pair{28_b,16_b},
      std::pair{24_b,20_b}, std::pair{28_b,20_b},
      std::pair{28_b,24_b});

  champsim::dynamic_extent lhs{16_b,8_b};
  champsim::dynamic_extent rhs{v,w};

  REQUIRE(champsim::relative_extent(lhs, rhs).upper == std::min(v,8_b)+8_b);
  REQUIRE(champsim::relative_extent(lhs, rhs).lower == std::min(w,8_b)+8_b);
}

TEST_CASE("Values can be translated to lower extents") {
  auto given = 0xabcd;
  champsim::dynamic_extent from{champsim::data::bits{24}, champsim::data::bits{8}};
  champsim::dynamic_extent to{champsim::data::bits{24}, champsim::data::bits{0}};
  auto expected = 0xabcd00;
  INFO((given << champsim::to_underlying(from.lower)));
  INFO((given << (champsim::to_underlying(from.lower) - champsim::to_underlying(to.lower))));
  REQUIRE(champsim::translate(given, from, to) == expected);
}

TEST_CASE("Values can be translated to higher extents") {
  auto given = 0xabcd;
  champsim::dynamic_extent from{champsim::data::bits{16}, champsim::data::bits{0}};
  champsim::dynamic_extent to{champsim::data::bits{16}, champsim::data::bits{8}};
  auto expected = 0xab;
  INFO((given >> champsim::to_underlying(to.lower)));
  INFO((given >> (champsim::to_underlying(to.lower) - champsim::to_underlying(from.lower))));
  REQUIRE(champsim::translate(given, from, to) == expected);
}
