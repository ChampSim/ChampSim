#include <catch.hpp>
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

TEST_CASE("bitmask correctly produces slice masks") {
  auto i = GENERATE(table<unsigned, unsigned>({
      {0,0}, {1,0}, {2,0}, {3,0}, {4,0}, {5,0}, {6,0}, {7,0}, {8,0}, {9,0}, {10,0}, {11,0}, {12,0}, {13,0}, {14,0}, {15,0}, {16,0},
      {20,0}, {24,0}, {28,0}, {32,0}, {40,0}, {48,0},
             {1,1}, {2,1}, {3,1}, {4,1}, {5,1}, {6,1}, {7,1}, {8,1}, {9,1}, {10,1}, {11,1}, {12,1}, {13,1}, {14,1}, {15,1}, {16,1},
             {20,1}, {24,1}, {28,1}, {32,1}, {40,1}, {48,1},
                    {2,2}, {3,2}, {4,2}, {5,2}, {6,2}, {7,2}, {8,2}, {9,2}, {10,2}, {11,2}, {12,2}, {13,2}, {14,2}, {15,2}, {16,2},
                    {20,2}, {24,2}, {28,2}, {32,2}, {40,2}, {48,2},
                           {3,3}, {4,3}, {5,3}, {6,3}, {7,3}, {8,3}, {9,3}, {10,3}, {11,3}, {12,3}, {13,3}, {14,3}, {15,3}, {16,3},
                           {20,3}, {24,3}, {28,3}, {32,3}, {40,3}, {48,3},
                                  {4,4}, {5,4}, {6,4}, {7,4}, {8,4}, {9,4}, {10,4}, {11,4}, {12,4}, {13,4}, {14,4}, {15,4}, {16,4},
                                  {20,4}, {24,4}, {28,4}, {32,4}, {40,4}, {48,4},
                                         {5,5}, {6,5}, {7,5}, {8,5}, {9,5}, {10,5}, {11,5}, {12,5}, {13,5}, {14,5}, {15,5}, {16,5},
                                         {20,5}, {24,5}, {28,5}, {32,5}, {40,5}, {48,5},
                                                {6,6}, {7,6}, {8,6}, {9,6}, {10,6}, {11,6}, {12,6}, {13,6}, {14,6}, {15,6}, {16,6},
                                                {20,6}, {24,6}, {28,6}, {32,6}, {40,6}, {48,6},
                                                       {7,7}, {8,7}, {9,7}, {10,7}, {11,7}, {12,7}, {13,7}, {14,7}, {15,7}, {16,7},
                                                       {20,7}, {24,7}, {28,7}, {32,7}, {40,7}, {48,7},
      {8,8}, {9,8}, {10,8},  {11,8},  {12,8},  {13,8},  {14,8},  {15,8},  {16,8},  {20,8},  {24,8},  {28,8},  {32,8},  {40,8},  {48,8},
             {9,9}, {10,9},  {11,9},  {12,9},  {13,9},  {14,9},  {15,9},  {16,9},  {20,9},  {24,9},  {28,9},  {32,9},  {40,9},  {48,9},
                    {10,10}, {11,10}, {12,10}, {13,10}, {14,10}, {15,10}, {16,10}, {20,10}, {24,10}, {28,10}, {32,10}, {40,10}, {48,10},
                             {11,11}, {12,11}, {13,11}, {14,11}, {15,11}, {16,11}, {20,11}, {24,11}, {28,11}, {32,11}, {40,11}, {48,11},
                                      {12,12}, {13,12}, {14,12}, {15,12}, {16,12}, {20,12}, {24,12}, {28,12}, {32,12}, {40,12}, {48,12},
                                               {13,13}, {14,13}, {15,13}, {16,13}, {20,13}, {24,13}, {28,13}, {32,13}, {40,13}, {48,13},
                                                        {14,14}, {15,14}, {16,14}, {20,14}, {24,14}, {28,14}, {32,14}, {40,14}, {48,14},
                                                                 {15,15}, {16,15}, {20,15}, {24,15}, {28,15}, {32,15}, {40,15}, {48,15},
                                                                          {16,16}, {20,16}, {24,16}, {28,16}, {32,16}, {40,16}, {48,16},
                                                                                   {20,20}, {24,20}, {28,20}, {32,20}, {40,20}, {48,20},
                                                                                            {24,24}, {28,24}, {32,24}, {40,24}, {48,24},
                                                                                                     {28,28}, {32,28}, {40,28}, {48,28},
                                                                                                              {32,32}, {40,32}, {48,32},
                                                                                                                       {40,40}, {48,40},
                                                                                                                                {48,48}
      }));
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

