#include <catch.hpp>
#include "operable.h"

namespace {
struct mock_operable : champsim::operable {
  using operable::operable;
  long operate() final { return 1; }
};
}

TEST_CASE("An operable with a scale of 1 operates every cycle") {
  constexpr double scale = 1;
  constexpr int num_cycles = 100;
  mock_operable uut{scale};

  for (int i = 0; i < num_cycles; ++i)
    uut._operate();

  REQUIRE(uut.current_cycle == num_cycles);
}

TEST_CASE("An operable with a scale greater than 1 skips occasional cycles") {
  constexpr double scale = 1.25;
  constexpr int num_cycles = 100;
  mock_operable uut{scale};

  for (int i = 0; i < num_cycles; ++i)
    uut._operate();

  REQUIRE(uut.current_cycle == (4*num_cycles)/5);
}

TEST_CASE("An operable with a scale greater than 2 skips multiple cycles") {
  constexpr double scale = 4;
  constexpr int num_cycles = 100;
  mock_operable uut{scale};

  for (int i = 0; i < num_cycles; ++i)
    uut._operate();

  REQUIRE(uut.current_cycle == num_cycles/4);
}
