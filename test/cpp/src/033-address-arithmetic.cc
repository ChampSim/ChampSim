#include "catch.hpp"
#include "address.h"

using namespace champsim::data::data_literals;

TEST_CASE("Address slice addition overflow works modulo 2^N") {
  CHECK(champsim::address_slice<champsim::static_extent<1_b,0_b>>{1} + 1 == champsim::address_slice<champsim::static_extent<1_b,0_b>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<2_b,0_b>>{3} + 1 == champsim::address_slice<champsim::static_extent<2_b,0_b>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<3_b,0_b>>{7} + 1 == champsim::address_slice<champsim::static_extent<3_b,0_b>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<4_b,0_b>>{15} + 1 == champsim::address_slice<champsim::static_extent<4_b,0_b>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<5_b,0_b>>{31} + 1 == champsim::address_slice<champsim::static_extent<5_b,0_b>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<6_b,0_b>>{63} + 1 == champsim::address_slice<champsim::static_extent<6_b,0_b>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<7_b,0_b>>{127} + 1 == champsim::address_slice<champsim::static_extent<7_b,0_b>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<8_b,0_b>>{255} + 1 == champsim::address_slice<champsim::static_extent<8_b,0_b>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<16_b,0_b>>{0xffff} + 1 == champsim::address_slice<champsim::static_extent<16_b,0_b>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<32_b,0_b>>{0xffff'ffff} + 1 == champsim::address_slice<champsim::static_extent<32_b,0_b>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<64_b,0_b>>{0xffff'ffff'ffff'ffff} + 1 == champsim::address_slice<champsim::static_extent<64_b,0_b>>{0});

  CHECK(champsim::address_slice<champsim::static_extent<1_b,0_b>>{0} + (-1) == champsim::address_slice<champsim::static_extent<1_b,0_b>>{1});
  CHECK(champsim::address_slice<champsim::static_extent<2_b,0_b>>{0} + (-1) == champsim::address_slice<champsim::static_extent<2_b,0_b>>{3});
  CHECK(champsim::address_slice<champsim::static_extent<3_b,0_b>>{0} + (-1) == champsim::address_slice<champsim::static_extent<3_b,0_b>>{7});
  CHECK(champsim::address_slice<champsim::static_extent<4_b,0_b>>{0} + (-1) == champsim::address_slice<champsim::static_extent<4_b,0_b>>{15});
  CHECK(champsim::address_slice<champsim::static_extent<5_b,0_b>>{0} + (-1) == champsim::address_slice<champsim::static_extent<5_b,0_b>>{31});
  CHECK(champsim::address_slice<champsim::static_extent<6_b,0_b>>{0} + (-1) == champsim::address_slice<champsim::static_extent<6_b,0_b>>{63});
  CHECK(champsim::address_slice<champsim::static_extent<7_b,0_b>>{0} + (-1) == champsim::address_slice<champsim::static_extent<7_b,0_b>>{127});
  CHECK(champsim::address_slice<champsim::static_extent<8_b,0_b>>{0} + (-1) == champsim::address_slice<champsim::static_extent<8_b,0_b>>{255});
  CHECK(champsim::address_slice<champsim::static_extent<16_b,0_b>>{0} + (-1) == champsim::address_slice<champsim::static_extent<16_b,0_b>>{0xffff});
  CHECK(champsim::address_slice<champsim::static_extent<32_b,0_b>>{0} + (-1) == champsim::address_slice<champsim::static_extent<32_b,0_b>>{0xffff'ffff});
  CHECK(champsim::address_slice<champsim::static_extent<64_b,0_b>>{0} + (-1) == champsim::address_slice<champsim::static_extent<64_b,0_b>>{0xffff'ffff'ffff'ffff});
}

TEST_CASE("Address slice subtraction overflow works modulo 2^N") {
  CHECK(champsim::address_slice<champsim::static_extent<1_b,0_b>>{0} - 1 == champsim::address_slice<champsim::static_extent<1_b,0_b>>{1});
  CHECK(champsim::address_slice<champsim::static_extent<2_b,0_b>>{0} - 1 == champsim::address_slice<champsim::static_extent<2_b,0_b>>{3});
  CHECK(champsim::address_slice<champsim::static_extent<3_b,0_b>>{0} - 1 == champsim::address_slice<champsim::static_extent<3_b,0_b>>{7});
  CHECK(champsim::address_slice<champsim::static_extent<4_b,0_b>>{0} - 1 == champsim::address_slice<champsim::static_extent<4_b,0_b>>{15});
  CHECK(champsim::address_slice<champsim::static_extent<5_b,0_b>>{0} - 1 == champsim::address_slice<champsim::static_extent<5_b,0_b>>{31});
  CHECK(champsim::address_slice<champsim::static_extent<6_b,0_b>>{0} - 1 == champsim::address_slice<champsim::static_extent<6_b,0_b>>{63});
  CHECK(champsim::address_slice<champsim::static_extent<7_b,0_b>>{0} - 1 == champsim::address_slice<champsim::static_extent<7_b,0_b>>{127});
  CHECK(champsim::address_slice<champsim::static_extent<8_b,0_b>>{0} - 1 == champsim::address_slice<champsim::static_extent<8_b,0_b>>{255});
  CHECK(champsim::address_slice<champsim::static_extent<16_b,0_b>>{0} - 1 == champsim::address_slice<champsim::static_extent<16_b,0_b>>{0xffff});
  CHECK(champsim::address_slice<champsim::static_extent<32_b,0_b>>{0} - 1 == champsim::address_slice<champsim::static_extent<32_b,0_b>>{0xffff'ffff});
  CHECK(champsim::address_slice<champsim::static_extent<64_b,0_b>>{0} - 1 == champsim::address_slice<champsim::static_extent<64_b,0_b>>{0xffff'ffff'ffff'ffff});

  CHECK(champsim::address_slice<champsim::static_extent<1_b,0_b>>{1} - (-1) == champsim::address_slice<champsim::static_extent<1_b,0_b>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<2_b,0_b>>{3} - (-1) == champsim::address_slice<champsim::static_extent<2_b,0_b>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<3_b,0_b>>{7} - (-1) == champsim::address_slice<champsim::static_extent<3_b,0_b>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<4_b,0_b>>{15} - (-1) == champsim::address_slice<champsim::static_extent<4_b,0_b>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<5_b,0_b>>{31} - (-1) == champsim::address_slice<champsim::static_extent<5_b,0_b>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<6_b,0_b>>{63} - (-1) == champsim::address_slice<champsim::static_extent<6_b,0_b>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<7_b,0_b>>{127} - (-1) == champsim::address_slice<champsim::static_extent<7_b,0_b>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<8_b,0_b>>{255} - (-1) == champsim::address_slice<champsim::static_extent<8_b,0_b>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<16_b,0_b>>{0xffff} - (-1) == champsim::address_slice<champsim::static_extent<16_b,0_b>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<32_b,0_b>>{0xffff'ffff} - (-1) == champsim::address_slice<champsim::static_extent<32_b,0_b>>{0});
  CHECK(champsim::address_slice<champsim::static_extent<64_b,0_b>>{0xffff'ffff'ffff'ffff} - (-1) == champsim::address_slice<champsim::static_extent<64_b,0_b>>{0});
}

TEST_CASE("Dynamic address slice addition overflow works modulo 2^N") {
  CHECK(champsim::address_slice{champsim::dynamic_extent{1_b,0_b},1} + 1 == champsim::address_slice{champsim::dynamic_extent{1_b,0_b},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{2_b,0_b},3} + 1 == champsim::address_slice{champsim::dynamic_extent{2_b,0_b},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{3_b,0_b},7} + 1 == champsim::address_slice{champsim::dynamic_extent{3_b,0_b},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{4_b,0_b},15} + 1 == champsim::address_slice{champsim::dynamic_extent{4_b,0_b},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{5_b,0_b},31} + 1 == champsim::address_slice{champsim::dynamic_extent{5_b,0_b},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{6_b,0_b},63} + 1 == champsim::address_slice{champsim::dynamic_extent{6_b,0_b},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{7_b,0_b},127} + 1 == champsim::address_slice{champsim::dynamic_extent{7_b,0_b},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{8_b,0_b},255} + 1 == champsim::address_slice{champsim::dynamic_extent{8_b,0_b},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{16_b,0_b},0xffff} + 1 == champsim::address_slice{champsim::dynamic_extent{16_b,0_b},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{32_b,0_b},0xffff'ffff} + 1 == champsim::address_slice{champsim::dynamic_extent{32_b,0_b},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{64_b,0_b},0xffff'ffff'ffff'ffff} + 1 == champsim::address_slice{champsim::dynamic_extent{64_b,0_b},0});

  CHECK(champsim::address_slice{champsim::dynamic_extent{1_b,0_b},0} + (-1) == champsim::address_slice{champsim::dynamic_extent{1_b,0_b},1});
  CHECK(champsim::address_slice{champsim::dynamic_extent{2_b,0_b},0} + (-1) == champsim::address_slice{champsim::dynamic_extent{2_b,0_b},3});
  CHECK(champsim::address_slice{champsim::dynamic_extent{3_b,0_b},0} + (-1) == champsim::address_slice{champsim::dynamic_extent{3_b,0_b},7});
  CHECK(champsim::address_slice{champsim::dynamic_extent{4_b,0_b},0} + (-1) == champsim::address_slice{champsim::dynamic_extent{4_b,0_b},15});
  CHECK(champsim::address_slice{champsim::dynamic_extent{5_b,0_b},0} + (-1) == champsim::address_slice{champsim::dynamic_extent{5_b,0_b},31});
  CHECK(champsim::address_slice{champsim::dynamic_extent{6_b,0_b},0} + (-1) == champsim::address_slice{champsim::dynamic_extent{6_b,0_b},63});
  CHECK(champsim::address_slice{champsim::dynamic_extent{7_b,0_b},0} + (-1) == champsim::address_slice{champsim::dynamic_extent{7_b,0_b},127});
  CHECK(champsim::address_slice{champsim::dynamic_extent{8_b,0_b},0} + (-1) == champsim::address_slice{champsim::dynamic_extent{8_b,0_b},255});
  CHECK(champsim::address_slice{champsim::dynamic_extent{16_b,0_b},0} + (-1) == champsim::address_slice{champsim::dynamic_extent{16_b,0_b},0xffff});
  CHECK(champsim::address_slice{champsim::dynamic_extent{32_b,0_b},0} + (-1) == champsim::address_slice{champsim::dynamic_extent{32_b,0_b},0xffff'ffff});
  CHECK(champsim::address_slice{champsim::dynamic_extent{64_b,0_b},0} + (-1) == champsim::address_slice{champsim::dynamic_extent{64_b,0_b},0xffff'ffff'ffff'ffff});
}

TEST_CASE("Dynamic address slice subtraction overflow works modulo 2^N") {
  CHECK(champsim::address_slice{champsim::dynamic_extent{1_b,0_b},0} - 1 == champsim::address_slice{champsim::dynamic_extent{1_b,0_b},1});
  CHECK(champsim::address_slice{champsim::dynamic_extent{2_b,0_b},0} - 1 == champsim::address_slice{champsim::dynamic_extent{2_b,0_b},3});
  CHECK(champsim::address_slice{champsim::dynamic_extent{3_b,0_b},0} - 1 == champsim::address_slice{champsim::dynamic_extent{3_b,0_b},7});
  CHECK(champsim::address_slice{champsim::dynamic_extent{4_b,0_b},0} - 1 == champsim::address_slice{champsim::dynamic_extent{4_b,0_b},15});
  CHECK(champsim::address_slice{champsim::dynamic_extent{5_b,0_b},0} - 1 == champsim::address_slice{champsim::dynamic_extent{5_b,0_b},31});
  CHECK(champsim::address_slice{champsim::dynamic_extent{6_b,0_b},0} - 1 == champsim::address_slice{champsim::dynamic_extent{6_b,0_b},63});
  CHECK(champsim::address_slice{champsim::dynamic_extent{7_b,0_b},0} - 1 == champsim::address_slice{champsim::dynamic_extent{7_b,0_b},127});
  CHECK(champsim::address_slice{champsim::dynamic_extent{8_b,0_b},0} - 1 == champsim::address_slice{champsim::dynamic_extent{8_b,0_b},255});
  CHECK(champsim::address_slice{champsim::dynamic_extent{16_b,0_b},0} - 1 == champsim::address_slice{champsim::dynamic_extent{16_b,0_b},0xffff});
  CHECK(champsim::address_slice{champsim::dynamic_extent{32_b,0_b},0} - 1 == champsim::address_slice{champsim::dynamic_extent{32_b,0_b},0xffff'ffff});
  CHECK(champsim::address_slice{champsim::dynamic_extent{64_b,0_b},0} - 1 == champsim::address_slice{champsim::dynamic_extent{64_b,0_b},0xffff'ffff'ffff'ffff});

  CHECK(champsim::address_slice{champsim::dynamic_extent{1_b,0_b},1} - (-1) == champsim::address_slice{champsim::dynamic_extent{1_b,0_b},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{2_b,0_b},3} - (-1) == champsim::address_slice{champsim::dynamic_extent{2_b,0_b},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{3_b,0_b},7} - (-1) == champsim::address_slice{champsim::dynamic_extent{3_b,0_b},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{4_b,0_b},15} - (-1) == champsim::address_slice{champsim::dynamic_extent{4_b,0_b},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{5_b,0_b},31} - (-1) == champsim::address_slice{champsim::dynamic_extent{5_b,0_b},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{6_b,0_b},63} - (-1) == champsim::address_slice{champsim::dynamic_extent{6_b,0_b},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{7_b,0_b},127} - (-1) == champsim::address_slice{champsim::dynamic_extent{7_b,0_b},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{8_b,0_b},255} - (-1) == champsim::address_slice{champsim::dynamic_extent{8_b,0_b},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{16_b,0_b},0xffff} - (-1) == champsim::address_slice{champsim::dynamic_extent{16_b,0_b},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{32_b,0_b},0xffff'ffff} - (-1) == champsim::address_slice{champsim::dynamic_extent{32_b,0_b},0});
  CHECK(champsim::address_slice{champsim::dynamic_extent{64_b,0_b},0xffff'ffff'ffff'ffff} - (-1) == champsim::address_slice{champsim::dynamic_extent{64_b,0_b},0});
}
