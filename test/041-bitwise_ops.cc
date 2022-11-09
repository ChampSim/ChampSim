#include "catch.hpp"
#include "util.h"
#include <bitset>

TEST_CASE("lg2 correctly identifies powers of 2") {
  auto i = GENERATE(range(0u,64u));
  REQUIRE(lg2((1ull << i)) == i);
}

TEST_CASE("bitmask correctly produces lower-order bits") {
  auto i = GENERATE(range(0u, 64u));
  REQUIRE(bitmask(i) < (1ull << i));
  REQUIRE(std::bitset<64>{bitmask(i)}.count() == i);
}

class TriangularGenerator : public Catch::Generators::IGenerator<std::pair<unsigned, unsigned>> {
  std::pair<unsigned, unsigned> val{};

  unsigned limit;
public:

    TriangularGenerator(unsigned limit): limit(limit) {}

    std::pair<unsigned, unsigned> const& get() const override;
    bool next() override {
      if (std::get<1>(val) >= std::get<0>(val)) {
        ++std::get<0>(val);
        std::get<1>(val) = 0;
      } else {
        ++std::get<1>(val);
      }

      return (std::get<0>(val) < limit);
    }
};

std::pair<unsigned, unsigned> const& TriangularGenerator::get() const {
    return val;
}

TEST_CASE("bitmask correctly produces slice masks") {
  auto i = GENERATE(Catch::Generators::GeneratorWrapper<std::pair<unsigned, unsigned>>(
        std::make_unique<TriangularGenerator>(64)
    ));
  REQUIRE(bitmask(std::get<0>(i), std::get<1>(i)) < (1ull << std::get<0>(i)));
  REQUIRE(std::bitset<64>{bitmask(std::get<0>(i), std::get<1>(i))}.count() == (std::get<0>(i) - std::get<1>(i)));
}

TEST_CASE("splice_bits performs correctly at the limits") {
  constexpr unsigned long long a{0xaaaaaaaaaaaaaaaa};
  constexpr unsigned long long b{0xbbbbbbbbbbbbbbbb};

  REQUIRE(splice_bits(a,b,64) == b);
  REQUIRE(splice_bits(a,b,0) == a);
}

TEST_CASE("splice_bits performs correctly in the middle") {
  constexpr unsigned long long a{0xaaaaaaaaaaaaaaaa};
  constexpr unsigned long long b{0xbbbbbbbbbbbbbbbb};

  REQUIRE(splice_bits(a,b,8) == 0xaaaaaaaaaaaaaabb);
  REQUIRE(splice_bits(a,b,16) == 0xaaaaaaaaaaaabbbb);
  REQUIRE(splice_bits(a,b,32) == 0xaaaaaaaabbbbbbbb);
  REQUIRE(splice_bits(a,0,8) == 0xaaaaaaaaaaaaaa00);
  REQUIRE(splice_bits(a,0,16) == 0xaaaaaaaaaaaa0000);
  REQUIRE(splice_bits(a,0,32) == 0xaaaaaaaa00000000);
  REQUIRE(splice_bits(b,0,8) == 0xbbbbbbbbbbbbbb00);
  REQUIRE(splice_bits(b,0,16) == 0xbbbbbbbbbbbb0000);
  REQUIRE(splice_bits(b,0,32) == 0xbbbbbbbb00000000);
  REQUIRE(splice_bits(0,b,8) == 0xbb);
  REQUIRE(splice_bits(0,b,16) == 0xbbbb);
  REQUIRE(splice_bits(0,b,32) == 0xbbbbbbbb);
}

TEST_CASE("lg2 correctly identifies powers of 2 in a constexpr") {
  STATIC_REQUIRE(lg2((1ull << 8)) == 8); // Sufficient to only test one, since the runtime check tests all
}

TEST_CASE("bitmask correctly produces lower-bit masks in a constexpr") {
  constexpr std::bitset<64> test_val{bitmask(8)};
  STATIC_REQUIRE(test_val[0]);
  STATIC_REQUIRE(test_val[1]);
  STATIC_REQUIRE(test_val[2]);
  STATIC_REQUIRE(test_val[3]);
  STATIC_REQUIRE(test_val[4]);
  STATIC_REQUIRE(test_val[5]);
  STATIC_REQUIRE(test_val[6]);
  STATIC_REQUIRE(test_val[7]);
}

TEST_CASE("bitmask correctly produces slice masks in a constexpr") {
  constexpr std::bitset<64> test_val{bitmask(8,2)};
  STATIC_REQUIRE_FALSE(test_val[0]);
  STATIC_REQUIRE_FALSE(test_val[1]);
  STATIC_REQUIRE(test_val[2]);
  STATIC_REQUIRE(test_val[3]);
  STATIC_REQUIRE(test_val[4]);
  STATIC_REQUIRE(test_val[5]);
  STATIC_REQUIRE(test_val[6]);
  STATIC_REQUIRE(test_val[7]);
}

