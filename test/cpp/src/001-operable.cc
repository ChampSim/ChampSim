#include <catch.hpp>
#include "operable.h"
#include "champsim_constants.h"

namespace {
struct mock_operable : champsim::operable {
  using operable::operable;
  void operate() {}
};
}

TEST_CASE("An operable with a scale of 1 operates every cycle") {
  champsim::chrono::global_clock_period period{1};
  constexpr int num_cycles = 100;
  mock_operable uut{period};

  for (int i = 0; i < num_cycles; ++i)
    uut._operate();

  REQUIRE(uut.current_cycle == num_cycles);
}

TEST_CASE("An operable with a scale greater than 1 skips occasional cycles") {
  std::chrono::duration<
    std::intmax_t,
    std::ratio_multiply<
      champsim::chrono::global_clock_period::period,
      std::ratio<150, 100>
    >
  > period{1};
  constexpr int num_cycles = 100;
  mock_operable uut{champsim::chrono::picoseconds{period}};

  for (int i = 0; i < num_cycles; ++i)
    uut._operate();

  REQUIRE(uut.current_cycle <= (2*num_cycles)/3 + 1);
  REQUIRE(uut.current_cycle >= (2*num_cycles)/3 - 1);
}

TEST_CASE("An operable with a scale greater than 2 skips multiple cycles") {
  champsim::chrono::global_clock_period period{4};
  constexpr int num_cycles = 100;
  mock_operable uut{period};

  for (int i = 0; i < num_cycles; ++i)
    uut._operate();

  REQUIRE(uut.current_cycle == num_cycles/4);
}
