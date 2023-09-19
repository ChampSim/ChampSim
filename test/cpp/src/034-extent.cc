#define CATCH_CONFIG_RUNTIME_STATIC_REQUIRE
#include <catch.hpp>

#include "extent.h"

TEMPLATE_TEST_CASE_SIG("The union of adjacent static extents is bounded by the maximum and minimum", "", ((std::size_t V), V), 4, 8, 12, 16, 20, 24, 28) {
  champsim::static_extent<32,V> lhs{};
  champsim::static_extent<V,0> rhs{};

  REQUIRE(champsim::extent_union(lhs, rhs).upper == 32);
  REQUIRE(champsim::extent_union(lhs, rhs).lower == 0);
}

TEMPLATE_TEST_CASE_SIG("Static extents that are subsets can be unioned", "", ((std::size_t V, std::size_t W), V, W), (8,4), (12,4), (16,4), (20,4), (24,4), (28,4), (12,8), (16,8), (20,8), (24,8), (28,8), (16,12), (20,12), (24,12), (28,12), (20,16), (24,16), (28,16),  (24,20), (28,20), (28,24)) {
  champsim::static_extent<32,0> lhs{};
  champsim::static_extent<V,W> rhs{};

  REQUIRE(champsim::extent_union(lhs, rhs).upper == 32);
  REQUIRE(champsim::extent_union(lhs, rhs).lower == 0);
}

TEST_CASE("The union of adjacent dynamic extents is bounded by the maximum and minimum") {
  auto v = GENERATE(as<std::size_t>{}, 4, 8, 12, 16, 20, 24, 28);
  champsim::dynamic_extent lhs{32, v};
  champsim::dynamic_extent rhs{v, 0};

  REQUIRE(champsim::extent_union(lhs, rhs).upper == 32);
  REQUIRE(champsim::extent_union(lhs, rhs).lower == 0);
}

TEST_CASE("Dynamic extents that are subsets can be unioned") {
  auto [v,w] = GENERATE(as<std::pair<std::size_t, std::size_t>>{},
      std::pair{8,4}, std::pair{12,4}, std::pair{16,4}, std::pair{20,4}, std::pair{24,4}, std::pair{28,4},
      std::pair{12,8}, std::pair{16,8}, std::pair{20,8}, std::pair{24,8}, std::pair{28,8},
      std::pair{16,12}, std::pair{20,12}, std::pair{24,12}, std::pair{28,12},
      std::pair{20,16}, std::pair{24,16}, std::pair{28,16},
      std::pair{24,20}, std::pair{28,20},
      std::pair{28,24});

  champsim::dynamic_extent lhs{32,0};
  champsim::dynamic_extent rhs{v,w};

  REQUIRE(champsim::extent_union(lhs, rhs).upper == 32);
  REQUIRE(champsim::extent_union(lhs, rhs).lower == 0);
}

TEMPLATE_TEST_CASE_SIG("The union of adjacent mismatched extents is bounded by the maximum and minimum", "", ((std::size_t V), V), 4, 8, 12, 16, 20, 24, 28) {
  champsim::static_extent<32,V> lhs{};
  champsim::dynamic_extent rhs{V,0};

  REQUIRE(champsim::extent_union(lhs, rhs).upper == 32);
  REQUIRE(champsim::extent_union(lhs, rhs).lower == 0);
  REQUIRE(champsim::extent_union(rhs, lhs).upper == 32);
  REQUIRE(champsim::extent_union(rhs, lhs).lower == 0);
}

TEST_CASE("Mismatched extents that are subsets can be unioned") {
  auto [v,w] = GENERATE(as<std::pair<std::size_t, std::size_t>>{},
      std::pair{8,4}, std::pair{12,4}, std::pair{16,4}, std::pair{20,4}, std::pair{24,4}, std::pair{28,4},
      std::pair{12,8}, std::pair{16,8}, std::pair{20,8}, std::pair{24,8}, std::pair{28,8},
      std::pair{16,12}, std::pair{20,12}, std::pair{24,12}, std::pair{28,12},
      std::pair{20,16}, std::pair{24,16}, std::pair{28,16},
      std::pair{24,20}, std::pair{28,20},
      std::pair{28,24});

  champsim::static_extent<32,0> lhs{};
  champsim::dynamic_extent rhs{v,w};

  REQUIRE(champsim::extent_union(lhs, rhs).upper == 32);
  REQUIRE(champsim::extent_union(lhs, rhs).lower == 0);
  REQUIRE(champsim::extent_union(rhs, lhs).upper == 32);
  REQUIRE(champsim::extent_union(rhs, lhs).lower == 0);
}

TEMPLATE_TEST_CASE_SIG("Static extents can be subextented", "", ((std::size_t V, std::size_t W), V, W), (8,4), (12,4), (16,4), (20,4), (24,4), (28,4), (12,8), (16,8), (20,8), (24,8), (28,8), (16,12), (20,12), (24,12), (28,12), (20,16), (24,16), (28,16),  (24,20), (28,20), (28,24)) {
  {
    champsim::static_extent<32,0> lhs{};
    champsim::static_extent<V,W> rhs{};

    REQUIRE(champsim::relative_extent(lhs, rhs).upper == V);
    REQUIRE(champsim::relative_extent(lhs, rhs).lower == W);
  }

  {
    champsim::static_extent<36,4> lhs{};
    champsim::static_extent<V,W> rhs{};

    REQUIRE(champsim::relative_extent(lhs, rhs).upper == V+4);
    REQUIRE(champsim::relative_extent(lhs, rhs).lower == W+4);
  }
}

TEMPLATE_TEST_CASE_SIG("Static extents do not overflow when subextented", "", ((std::size_t V, std::size_t W), V, W), (8,4), (12,4), (16,4), (20,4), (24,4), (28,4), (12,8), (16,8), (20,8), (24,8), (28,8), (16,12), (20,12), (24,12), (28,12), (20,16), (24,16), (28,16),  (24,20), (28,20), (28,24)) {
  champsim::static_extent<16,8> lhs{};
  champsim::static_extent<V,W> rhs{};

  REQUIRE(champsim::relative_extent(lhs, rhs).upper == std::min(V,std::size_t{8})+8);
  REQUIRE(champsim::relative_extent(lhs, rhs).lower == std::min(W,std::size_t{8})+8);
}

TEST_CASE("Dynamic extents can be subextented") {
  auto [v,w] = GENERATE(as<std::pair<std::size_t, std::size_t>>{},
      std::pair{8,4}, std::pair{12,4}, std::pair{16,4}, std::pair{20,4}, std::pair{24,4}, std::pair{28,4},
      std::pair{12,8}, std::pair{16,8}, std::pair{20,8}, std::pair{24,8}, std::pair{28,8},
      std::pair{16,12}, std::pair{20,12}, std::pair{24,12}, std::pair{28,12},
      std::pair{20,16}, std::pair{24,16}, std::pair{28,16},
      std::pair{24,20}, std::pair{28,20},
      std::pair{28,24});

  {
    champsim::dynamic_extent lhs{32,0};
    champsim::dynamic_extent rhs{v,w};

    REQUIRE(champsim::relative_extent(lhs, rhs).upper == v);
    REQUIRE(champsim::relative_extent(lhs, rhs).lower == w);
  }

  {
    champsim::dynamic_extent lhs{36,4};
    champsim::dynamic_extent rhs{v,w};

    REQUIRE(champsim::relative_extent(lhs, rhs).upper == v+4);
    REQUIRE(champsim::relative_extent(lhs, rhs).lower == w+4);
  }
}

TEST_CASE("Dynamic extents do not overflow when subextented") {
  auto [v,w] = GENERATE(as<std::pair<std::size_t, std::size_t>>{},
      std::pair{8,4}, std::pair{12,4}, std::pair{16,4}, std::pair{20,4}, std::pair{24,4}, std::pair{28,4},
      std::pair{12,8}, std::pair{16,8}, std::pair{20,8}, std::pair{24,8}, std::pair{28,8},
      std::pair{16,12}, std::pair{20,12}, std::pair{24,12}, std::pair{28,12},
      std::pair{20,16}, std::pair{24,16}, std::pair{28,16},
      std::pair{24,20}, std::pair{28,20},
      std::pair{28,24});

  champsim::dynamic_extent lhs{16,8};
  champsim::dynamic_extent rhs{v,w};

  REQUIRE(champsim::relative_extent(lhs, rhs).upper == std::min(v,std::size_t{8})+8);
  REQUIRE(champsim::relative_extent(lhs, rhs).lower == std::min(w,std::size_t{8})+8);
}
