#include "catch.hpp"
#include "util/bits.h"
#include <bitset>

TEST_CASE("lg2 correctly identifies powers of 2") {
  auto i = GENERATE(range(0u,64u));
  REQUIRE(champsim::lg2((1ull << i)) == i);
}

TEST_CASE("bitmask correctly produces lower-order bits") {
  auto i = GENERATE(range(0u, 64u));
  REQUIRE(champsim::bitmask(i) < (1ull << i));
  REQUIRE(std::bitset<64>{champsim::bitmask(i)}.count() == i);
}

class TriangularGenerator : public Catch::Generators::IGenerator<std::pair<unsigned, unsigned>> {
  std::pair<unsigned, unsigned> val{};

  unsigned limit;
public:

    TriangularGenerator(unsigned max): limit(max) {}

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
  REQUIRE(champsim::bitmask(std::get<0>(i), std::get<1>(i)) < (1ull << std::get<0>(i)));
  REQUIRE(std::bitset<64>{champsim::bitmask(std::get<0>(i), std::get<1>(i))}.count() == (std::get<0>(i) - std::get<1>(i)));
}

TEST_CASE("splice_bits performs correctly at the limits") {
  constexpr unsigned long long a{0xaaaaaaaaaaaaaaaa};
  constexpr unsigned long long b{0xbbbbbbbbbbbbbbbb};

  REQUIRE(champsim::splice_bits(a,b,64) == b);
  REQUIRE(champsim::splice_bits(a,b,0) == a);
}

TEST_CASE("splice_bits performs correctly in the middle") {
  constexpr unsigned long long a{0xaaaaaaaaaaaaaaaa};
  constexpr unsigned long long b{0xbbbbbbbbbbbbbbbb};

  REQUIRE(champsim::splice_bits(a,b,8) == 0xaaaaaaaaaaaaaabb);
  REQUIRE(champsim::splice_bits(a,b,16) == 0xaaaaaaaaaaaabbbb);
  REQUIRE(champsim::splice_bits(a,b,32) == 0xaaaaaaaabbbbbbbb);
  REQUIRE(champsim::splice_bits(a,0,8) == 0xaaaaaaaaaaaaaa00);
  REQUIRE(champsim::splice_bits(a,0,16) == 0xaaaaaaaaaaaa0000);
  REQUIRE(champsim::splice_bits(a,0,32) == 0xaaaaaaaa00000000);
  REQUIRE(champsim::splice_bits(b,0,8) == 0xbbbbbbbbbbbbbb00);
  REQUIRE(champsim::splice_bits(b,0,16) == 0xbbbbbbbbbbbb0000);
  REQUIRE(champsim::splice_bits(b,0,32) == 0xbbbbbbbb00000000);
  REQUIRE(champsim::splice_bits(0,b,8) == 0xbb);
  REQUIRE(champsim::splice_bits(0,b,16) == 0xbbbb);
  REQUIRE(champsim::splice_bits(0,b,32) == 0xbbbbbbbb);
}

TEST_CASE("splice_bits performs partial masks correctly in the middle") {
  constexpr unsigned long long a{0xaaaaaaaaaaaaaaaa};
  constexpr unsigned long long b{0xbbbbbbbbbbbbbbbb};

  REQUIRE(champsim::splice_bits(a,b,8,4) == 0xaaaaaaaaaaaaaaba);
  REQUIRE(champsim::splice_bits(a,b,16,4) == 0xaaaaaaaaaaaabbba);
  REQUIRE(champsim::splice_bits(a,b,16,8) == 0xaaaaaaaaaaaabbaa);
  REQUIRE(champsim::splice_bits(a,b,32,8) == 0xaaaaaaaabbbbbbaa);
  REQUIRE(champsim::splice_bits(a,b,32,16) == 0xaaaaaaaabbbbaaaa);
  REQUIRE(champsim::splice_bits(a,0,8,4) == 0xaaaaaaaaaaaaaa0a);
  REQUIRE(champsim::splice_bits(a,0,16,4) == 0xaaaaaaaaaaaa000a);
  REQUIRE(champsim::splice_bits(a,0,16,8) == 0xaaaaaaaaaaaa00aa);
  REQUIRE(champsim::splice_bits(a,0,32,8) == 0xaaaaaaaa000000aa);
  REQUIRE(champsim::splice_bits(a,0,32,16) == 0xaaaaaaaa0000aaaa);
  REQUIRE(champsim::splice_bits(b,0,8,4) == 0xbbbbbbbbbbbbbb0b);
  REQUIRE(champsim::splice_bits(b,0,16,4) == 0xbbbbbbbbbbbb000b);
  REQUIRE(champsim::splice_bits(b,0,16,8) == 0xbbbbbbbbbbbb00bb);
  REQUIRE(champsim::splice_bits(b,0,32,8) == 0xbbbbbbbb000000bb);
  REQUIRE(champsim::splice_bits(b,0,32,16) == 0xbbbbbbbb0000bbbb);
  REQUIRE(champsim::splice_bits(0,b,8,4) == 0xb0);
  REQUIRE(champsim::splice_bits(0,b,16,4) == 0xbbb0);
  REQUIRE(champsim::splice_bits(0,b,16,8) == 0xbb00);
  REQUIRE(champsim::splice_bits(0,b,32,8) == 0xbbbbbb00);
  REQUIRE(champsim::splice_bits(0,b,32,16) == 0xbbbb0000);
}

TEST_CASE("lg2 correctly identifies powers of 2 in a constexpr") {
  STATIC_REQUIRE(champsim::lg2((1ull << 8)) == 8); // Sufficient to only test one, since the runtime check tests all
}

TEST_CASE("bitmask correctly produces lower-bit masks in a constexpr") {
  constexpr std::bitset<64> test_val{champsim::bitmask(8)};
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
  constexpr std::bitset<64> test_val{champsim::bitmask(8,2)};
  STATIC_REQUIRE_FALSE(test_val[0]);
  STATIC_REQUIRE_FALSE(test_val[1]);
  STATIC_REQUIRE(test_val[2]);
  STATIC_REQUIRE(test_val[3]);
  STATIC_REQUIRE(test_val[4]);
  STATIC_REQUIRE(test_val[5]);
  STATIC_REQUIRE(test_val[6]);
  STATIC_REQUIRE(test_val[7]);
}

