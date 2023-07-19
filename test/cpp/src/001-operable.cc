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
  champsim::chrono::clock global_clock{};
  champsim::chrono::clock::duration period{100};
  constexpr int num_cycles = 100;
  mock_operable uut{period};

  for (int i = 0; i < num_cycles; ++i) {
    global_clock.tick(period);
    uut.operate_on(global_clock);
  }

  REQUIRE(uut.current_cycle == num_cycles);
}

TEST_CASE("An operable with a scale greater than 1 skips occasional cycles") {
  champsim::chrono::clock global_clock{};
  champsim::chrono::clock::duration period{150};
  constexpr int num_cycles = 100;
  mock_operable uut{period};

  for (int i = 0; i < num_cycles; ++i) {
    global_clock.tick(champsim::chrono::picoseconds{100});
    uut.operate_on(global_clock);
  }

  REQUIRE(uut.current_cycle <= (2*num_cycles)/3 + 1);
  REQUIRE(uut.current_cycle >= (2*num_cycles)/3 - 1);
}

TEST_CASE("An operable with a scale greater than 2 skips multiple cycles") {
  champsim::chrono::clock global_clock{};
  champsim::chrono::clock::duration period{400};
  constexpr int num_cycles = 100;
  mock_operable uut{period};

  for (int i = 0; i < num_cycles; ++i) {
    global_clock.tick(champsim::chrono::picoseconds{100});
    uut.operate_on(global_clock);
  }

  REQUIRE(uut.current_cycle == num_cycles/4);
}
