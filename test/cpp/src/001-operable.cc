#include <catch.hpp>
#include "operable.h"

namespace {
struct mock_operable : champsim::operable {
  using operable::operable;
  void operate() {}
};
}

TEST_CASE("An operable with a scale of 1 operates every cycle") {
  champsim::chrono::picoseconds period{1};
  constexpr int num_cycles = 100;
  mock_operable uut{period, champsim::chrono::picoseconds{1}};

  for (int i = 0; i < num_cycles; ++i)
    uut._operate();

  REQUIRE(uut.current_cycle == num_cycles);
}

TEST_CASE("An operable with a scale greater than 1 skips occasional cycles") {
  champsim::chrono::picoseconds period{125};
  constexpr int num_cycles = 100;
  mock_operable uut{period, champsim::chrono::picoseconds{100}};

  for (int i = 0; i < num_cycles; ++i)
    uut._operate();

  REQUIRE(uut.current_cycle == (4*num_cycles)/5);
}

TEST_CASE("An operable with a scale greater than 2 skips multiple cycles") {
  champsim::chrono::picoseconds period{4};
  constexpr int num_cycles = 100;
  mock_operable uut{period, champsim::chrono::picoseconds{1}};

  for (int i = 0; i < num_cycles; ++i)
    uut._operate();

  REQUIRE(uut.current_cycle == num_cycles/4);
}
