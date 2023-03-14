#include <catch.hpp>

#include "repeatable.h"

namespace {
  template <typename F>
  struct mock_repeatable {
    std::string trace_string = "TESTtestTEST";
    bool ever_repeats;
    F callback;

    bool eof() const { return ever_repeats; }
    void restart() { callback(); }

    ooo_model_instr operator()() { return ooo_model_instr{0, input_instr{}}; }

    explicit mock_repeatable(bool r, F&& func) : ever_repeats(r), callback(func) {}
  };
}

TEST_CASE("A repeatable repeats") {
  bool restart_called = false;
  champsim::repeatable uut{mock_repeatable{true, [&](){ restart_called = true; }}};

  (void)uut();
  REQUIRE(restart_called);
}
