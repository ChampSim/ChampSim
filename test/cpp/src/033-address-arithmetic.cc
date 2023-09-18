#include "catch.hpp"
#include "address.h"

TEST_CASE("Address slice addition overflow works modulo 2^N") {
  CHECK(champsim::address_slice<champsim::static_extent<1,0>>{1} + 1 == champsim::address_slice<champsim::static_extent<1,0>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<2,0>>{3} + 1 == champsim::address_slice<champsim::static_extent<2,0>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<3,0>>{7} + 1 == champsim::address_slice<champsim::static_extent<3,0>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<4,0>>{15} + 1 == champsim::address_slice<champsim::static_extent<4,0>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<5,0>>{31} + 1 == champsim::address_slice<champsim::static_extent<5,0>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<6,0>>{63} + 1 == champsim::address_slice<champsim::static_extent<6,0>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<7,0>>{127} + 1 == champsim::address_slice<champsim::static_extent<7,0>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<8,0>>{255} + 1 == champsim::address_slice<champsim::static_extent<8,0>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<16,0>>{0xffff} + 1 == champsim::address_slice<champsim::static_extent<16,0>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<32,0>>{0xffff'ffff} + 1 == champsim::address_slice<champsim::static_extent<32,0>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<64,0>>{0xffff'ffff'ffff'ffff} + 1 == champsim::address_slice<champsim::static_extent<64,0>>{0});

  CHECK(champsim::address_slice<champsim::static_extent<1,0>>{0} + (-1) == champsim::address_slice<champsim::static_extent<1,0>>{1});
  CHECK(champsim::address_slice<champsim::static_extent<2,0>>{0} + (-1) == champsim::address_slice<champsim::static_extent<2,0>>{3});
  CHECK(champsim::address_slice<champsim::static_extent<3,0>>{0} + (-1) == champsim::address_slice<champsim::static_extent<3,0>>{7});
  CHECK(champsim::address_slice<champsim::static_extent<4,0>>{0} + (-1) == champsim::address_slice<champsim::static_extent<4,0>>{15});
  CHECK(champsim::address_slice<champsim::static_extent<5,0>>{0} + (-1) == champsim::address_slice<champsim::static_extent<5,0>>{31});
  CHECK(champsim::address_slice<champsim::static_extent<6,0>>{0} + (-1) == champsim::address_slice<champsim::static_extent<6,0>>{63});
  CHECK(champsim::address_slice<champsim::static_extent<7,0>>{0} + (-1) == champsim::address_slice<champsim::static_extent<7,0>>{127});
  CHECK(champsim::address_slice<champsim::static_extent<8,0>>{0} + (-1) == champsim::address_slice<champsim::static_extent<8,0>>{255});
  CHECK(champsim::address_slice<champsim::static_extent<16,0>>{0} + (-1) == champsim::address_slice<champsim::static_extent<16,0>>{0xffff});
  CHECK(champsim::address_slice<champsim::static_extent<32,0>>{0} + (-1) == champsim::address_slice<champsim::static_extent<32,0>>{0xffff'ffff});
  CHECK(champsim::address_slice<champsim::static_extent<64,0>>{0} + (-1) == champsim::address_slice<champsim::static_extent<64,0>>{0xffff'ffff'ffff'ffff});
}

TEST_CASE("Address slice subtraction overflow works modulo 2^N") {
  CHECK(champsim::address_slice<champsim::static_extent<1,0>>{0} - 1 == champsim::address_slice<champsim::static_extent<1,0>>{1});
  CHECK(champsim::address_slice<champsim::static_extent<2,0>>{0} - 1 == champsim::address_slice<champsim::static_extent<2,0>>{3});
  CHECK(champsim::address_slice<champsim::static_extent<3,0>>{0} - 1 == champsim::address_slice<champsim::static_extent<3,0>>{7});
  CHECK(champsim::address_slice<champsim::static_extent<4,0>>{0} - 1 == champsim::address_slice<champsim::static_extent<4,0>>{15});
  CHECK(champsim::address_slice<champsim::static_extent<5,0>>{0} - 1 == champsim::address_slice<champsim::static_extent<5,0>>{31});
  CHECK(champsim::address_slice<champsim::static_extent<6,0>>{0} - 1 == champsim::address_slice<champsim::static_extent<6,0>>{63});
  CHECK(champsim::address_slice<champsim::static_extent<7,0>>{0} - 1 == champsim::address_slice<champsim::static_extent<7,0>>{127});
  CHECK(champsim::address_slice<champsim::static_extent<8,0>>{0} - 1 == champsim::address_slice<champsim::static_extent<8,0>>{255});
  CHECK(champsim::address_slice<champsim::static_extent<16,0>>{0} - 1 == champsim::address_slice<champsim::static_extent<16,0>>{0xffff});
  CHECK(champsim::address_slice<champsim::static_extent<32,0>>{0} - 1 == champsim::address_slice<champsim::static_extent<32,0>>{0xffff'ffff});
  CHECK(champsim::address_slice<champsim::static_extent<64,0>>{0} - 1 == champsim::address_slice<champsim::static_extent<64,0>>{0xffff'ffff'ffff'ffff});

  CHECK(champsim::address_slice<champsim::static_extent<1,0>>{1} - (-1) == champsim::address_slice<champsim::static_extent<1,0>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<2,0>>{3} - (-1) == champsim::address_slice<champsim::static_extent<2,0>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<3,0>>{7} - (-1) == champsim::address_slice<champsim::static_extent<3,0>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<4,0>>{15} - (-1) == champsim::address_slice<champsim::static_extent<4,0>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<5,0>>{31} - (-1) == champsim::address_slice<champsim::static_extent<5,0>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<6,0>>{63} - (-1) == champsim::address_slice<champsim::static_extent<6,0>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<7,0>>{127} - (-1) == champsim::address_slice<champsim::static_extent<7,0>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<8,0>>{255} - (-1) == champsim::address_slice<champsim::static_extent<8,0>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<16,0>>{0xffff} - (-1) == champsim::address_slice<champsim::static_extent<16,0>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<32,0>>{0xffff'ffff} - (-1) == champsim::address_slice<champsim::static_extent<32,0>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<64,0>>{0xffff'ffff'ffff'ffff} - (-1) == champsim::address_slice<champsim::static_extent<64,0>>{0});
}

TEST_CASE("Dynamic address slice addition overflow works modulo 2^N") {
  CHECK(champsim::address_slice{champsim::dynamic_extent{1,0},1} + 1 == champsim::address_slice{champsim::dynamic_extent{1,0},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{2,0},3} + 1 == champsim::address_slice{champsim::dynamic_extent{2,0},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{3,0},7} + 1 == champsim::address_slice{champsim::dynamic_extent{3,0},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{4,0},15} + 1 == champsim::address_slice{champsim::dynamic_extent{4,0},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{5,0},31} + 1 == champsim::address_slice{champsim::dynamic_extent{5,0},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{6,0},63} + 1 == champsim::address_slice{champsim::dynamic_extent{6,0},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{7,0},127} + 1 == champsim::address_slice{champsim::dynamic_extent{7,0},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{8,0},255} + 1 == champsim::address_slice{champsim::dynamic_extent{8,0},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{16,0},0xffff} + 1 == champsim::address_slice{champsim::dynamic_extent{16,0},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{32,0},0xffff'ffff} + 1 == champsim::address_slice{champsim::dynamic_extent{32,0},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{64,0},0xffff'ffff'ffff'ffff} + 1 == champsim::address_slice{champsim::dynamic_extent{64,0},0});

  CHECK(champsim::address_slice{champsim::dynamic_extent{1,0},0} + (-1) == champsim::address_slice{champsim::dynamic_extent{1,0},1});
  CHECK(champsim::address_slice{champsim::dynamic_extent{2,0},0} + (-1) == champsim::address_slice{champsim::dynamic_extent{2,0},3});
  CHECK(champsim::address_slice{champsim::dynamic_extent{3,0},0} + (-1) == champsim::address_slice{champsim::dynamic_extent{3,0},7});
  CHECK(champsim::address_slice{champsim::dynamic_extent{4,0},0} + (-1) == champsim::address_slice{champsim::dynamic_extent{4,0},15});
  CHECK(champsim::address_slice{champsim::dynamic_extent{5,0},0} + (-1) == champsim::address_slice{champsim::dynamic_extent{5,0},31});
  CHECK(champsim::address_slice{champsim::dynamic_extent{6,0},0} + (-1) == champsim::address_slice{champsim::dynamic_extent{6,0},63});
  CHECK(champsim::address_slice{champsim::dynamic_extent{7,0},0} + (-1) == champsim::address_slice{champsim::dynamic_extent{7,0},127});
  CHECK(champsim::address_slice{champsim::dynamic_extent{8,0},0} + (-1) == champsim::address_slice{champsim::dynamic_extent{8,0},255});
  CHECK(champsim::address_slice{champsim::dynamic_extent{16,0},0} + (-1) == champsim::address_slice{champsim::dynamic_extent{16,0},0xffff});
  CHECK(champsim::address_slice{champsim::dynamic_extent{32,0},0} + (-1) == champsim::address_slice{champsim::dynamic_extent{32,0},0xffff'ffff});
  CHECK(champsim::address_slice{champsim::dynamic_extent{64,0},0} + (-1) == champsim::address_slice{champsim::dynamic_extent{64,0},0xffff'ffff'ffff'ffff});
}

TEST_CASE("Dynamic address slice subtraction overflow works modulo 2^N") {
  CHECK(champsim::address_slice{champsim::dynamic_extent{1,0},0} - 1 == champsim::address_slice{champsim::dynamic_extent{1,0},1});
  CHECK(champsim::address_slice{champsim::dynamic_extent{2,0},0} - 1 == champsim::address_slice{champsim::dynamic_extent{2,0},3});
  CHECK(champsim::address_slice{champsim::dynamic_extent{3,0},0} - 1 == champsim::address_slice{champsim::dynamic_extent{3,0},7});
  CHECK(champsim::address_slice{champsim::dynamic_extent{4,0},0} - 1 == champsim::address_slice{champsim::dynamic_extent{4,0},15});
  CHECK(champsim::address_slice{champsim::dynamic_extent{5,0},0} - 1 == champsim::address_slice{champsim::dynamic_extent{5,0},31});
  CHECK(champsim::address_slice{champsim::dynamic_extent{6,0},0} - 1 == champsim::address_slice{champsim::dynamic_extent{6,0},63});
  CHECK(champsim::address_slice{champsim::dynamic_extent{7,0},0} - 1 == champsim::address_slice{champsim::dynamic_extent{7,0},127});
  CHECK(champsim::address_slice{champsim::dynamic_extent{8,0},0} - 1 == champsim::address_slice{champsim::dynamic_extent{8,0},255});
  CHECK(champsim::address_slice{champsim::dynamic_extent{16,0},0} - 1 == champsim::address_slice{champsim::dynamic_extent{16,0},0xffff});
  CHECK(champsim::address_slice{champsim::dynamic_extent{32,0},0} - 1 == champsim::address_slice{champsim::dynamic_extent{32,0},0xffff'ffff});
  CHECK(champsim::address_slice{champsim::dynamic_extent{64,0},0} - 1 == champsim::address_slice{champsim::dynamic_extent{64,0},0xffff'ffff'ffff'ffff});

  CHECK(champsim::address_slice{champsim::dynamic_extent{1,0},1} - (-1) == champsim::address_slice{champsim::dynamic_extent{1,0},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{2,0},3} - (-1) == champsim::address_slice{champsim::dynamic_extent{2,0},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{3,0},7} - (-1) == champsim::address_slice{champsim::dynamic_extent{3,0},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{4,0},15} - (-1) == champsim::address_slice{champsim::dynamic_extent{4,0},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{5,0},31} - (-1) == champsim::address_slice{champsim::dynamic_extent{5,0},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{6,0},63} - (-1) == champsim::address_slice{champsim::dynamic_extent{6,0},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{7,0},127} - (-1) == champsim::address_slice{champsim::dynamic_extent{7,0},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{8,0},255} - (-1) == champsim::address_slice{champsim::dynamic_extent{8,0},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{16,0},0xffff} - (-1) == champsim::address_slice{champsim::dynamic_extent{16,0},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{32,0},0xffff'ffff} - (-1) == champsim::address_slice{champsim::dynamic_extent{32,0},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{64,0},0xffff'ffff'ffff'ffff} - (-1) == champsim::address_slice{champsim::dynamic_extent{64,0},0});
}
