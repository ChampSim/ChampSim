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
  auto func = [&](){ restart_called = true; };
  champsim::repeatable<mock_repeatable<decltype(func)>> uut{true, func};

  (void)uut();
  REQUIRE(restart_called);
}

namespace {
  template <typename T>
  struct configurable_repeatable {
    std::string trace_string = "TESTtestTEST";
    bool eof() const { return true; }
    void restart() {}

    T operator()() { return T{}; }
  };

  struct dummy_return_t {};
}

TEST_CASE("A repeatable's generator takes the type of the underlying generator") {
  STATIC_REQUIRE(std::is_same_v<
      std::string,
      std::invoke_result_t<
        champsim::repeatable< ::configurable_repeatable<std::string> >
      >
    >);

  STATIC_REQUIRE(std::is_same_v<
      ::dummy_return_t,
      std::invoke_result_t<
        champsim::repeatable< ::configurable_repeatable<::dummy_return_t> >
      >
    >);
}
