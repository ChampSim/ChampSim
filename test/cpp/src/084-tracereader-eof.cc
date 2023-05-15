#include <catch.hpp>

#include "tracereader.h"

TEST_CASE("A tracereader can be constructed from a type without an eof() member function") {
  champsim::tracereader uut{[](){ return ooo_model_instr{0, input_instr{}}; }};
  REQUIRE_FALSE(uut.eof());
  (void)uut();
  REQUIRE_FALSE(uut.eof());
}

