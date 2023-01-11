#include "catch.hpp"
#include "address.h"

TEST_CASE("Address slice addition overflow works modulo 2^N") {
  CHECK(champsim::address_slice<1,0>{1} + 1 == champsim::address_slice<1,0>{0});
  CHECK(champsim::address_slice<2,0>{3} + 1 == champsim::address_slice<2,0>{0});
  CHECK(champsim::address_slice<3,0>{7} + 1 == champsim::address_slice<3,0>{0});
  CHECK(champsim::address_slice<4,0>{15} + 1 == champsim::address_slice<4,0>{0});
  CHECK(champsim::address_slice<5,0>{31} + 1 == champsim::address_slice<5,0>{0});
  CHECK(champsim::address_slice<6,0>{63} + 1 == champsim::address_slice<6,0>{0});
  CHECK(champsim::address_slice<7,0>{127} + 1 == champsim::address_slice<7,0>{0});
  CHECK(champsim::address_slice<8,0>{255} + 1 == champsim::address_slice<8,0>{0});
  CHECK(champsim::address_slice<16,0>{0xffff} + 1 == champsim::address_slice<16,0>{0});
  CHECK(champsim::address_slice<32,0>{0xffff'ffff} + 1 == champsim::address_slice<32,0>{0});
  CHECK(champsim::address_slice<64,0>{0xffff'ffff'ffff'ffff} + 1 == champsim::address_slice<64,0>{0});

  CHECK(champsim::address_slice<1,0>{0} + (-1) == champsim::address_slice<1,0>{1});
  CHECK(champsim::address_slice<2,0>{0} + (-1) == champsim::address_slice<2,0>{3});
  CHECK(champsim::address_slice<3,0>{0} + (-1) == champsim::address_slice<3,0>{7});
  CHECK(champsim::address_slice<4,0>{0} + (-1) == champsim::address_slice<4,0>{15});
  CHECK(champsim::address_slice<5,0>{0} + (-1) == champsim::address_slice<5,0>{31});
  CHECK(champsim::address_slice<6,0>{0} + (-1) == champsim::address_slice<6,0>{63});
  CHECK(champsim::address_slice<7,0>{0} + (-1) == champsim::address_slice<7,0>{127});
  CHECK(champsim::address_slice<8,0>{0} + (-1) == champsim::address_slice<8,0>{255});
  CHECK(champsim::address_slice<16,0>{0} + (-1) == champsim::address_slice<16,0>{0xffff});
  CHECK(champsim::address_slice<32,0>{0} + (-1) == champsim::address_slice<32,0>{0xffff'ffff});
  CHECK(champsim::address_slice<64,0>{0} + (-1) == champsim::address_slice<64,0>{0xffff'ffff'ffff'ffff});
}

TEST_CASE("Address slice subtraction overflow works modulo 2^N") {
  CHECK(champsim::address_slice<1,0>{0} - 1 == champsim::address_slice<1,0>{1});
  CHECK(champsim::address_slice<2,0>{0} - 1 == champsim::address_slice<2,0>{3});
  CHECK(champsim::address_slice<3,0>{0} - 1 == champsim::address_slice<3,0>{7});
  CHECK(champsim::address_slice<4,0>{0} - 1 == champsim::address_slice<4,0>{15});
  CHECK(champsim::address_slice<5,0>{0} - 1 == champsim::address_slice<5,0>{31});
  CHECK(champsim::address_slice<6,0>{0} - 1 == champsim::address_slice<6,0>{63});
  CHECK(champsim::address_slice<7,0>{0} - 1 == champsim::address_slice<7,0>{127});
  CHECK(champsim::address_slice<8,0>{0} - 1 == champsim::address_slice<8,0>{255});
  CHECK(champsim::address_slice<16,0>{0} - 1 == champsim::address_slice<16,0>{0xffff});
  CHECK(champsim::address_slice<32,0>{0} - 1 == champsim::address_slice<32,0>{0xffff'ffff});
  CHECK(champsim::address_slice<64,0>{0} - 1 == champsim::address_slice<64,0>{0xffff'ffff'ffff'ffff});

  CHECK(champsim::address_slice<1,0>{1} - (-1) == champsim::address_slice<1,0>{0});
  CHECK(champsim::address_slice<2,0>{3} - (-1) == champsim::address_slice<2,0>{0});
  CHECK(champsim::address_slice<3,0>{7} - (-1) == champsim::address_slice<3,0>{0});
  CHECK(champsim::address_slice<4,0>{15} - (-1) == champsim::address_slice<4,0>{0});
  CHECK(champsim::address_slice<5,0>{31} - (-1) == champsim::address_slice<5,0>{0});
  CHECK(champsim::address_slice<6,0>{63} - (-1) == champsim::address_slice<6,0>{0});
  CHECK(champsim::address_slice<7,0>{127} - (-1) == champsim::address_slice<7,0>{0});
  CHECK(champsim::address_slice<8,0>{255} - (-1) == champsim::address_slice<8,0>{0});
  CHECK(champsim::address_slice<16,0>{0xffff} - (-1) == champsim::address_slice<16,0>{0});
  CHECK(champsim::address_slice<32,0>{0xffff'ffff} - (-1) == champsim::address_slice<32,0>{0});
  CHECK(champsim::address_slice<64,0>{0xffff'ffff'ffff'ffff} - (-1) == champsim::address_slice<64,0>{0});
}
