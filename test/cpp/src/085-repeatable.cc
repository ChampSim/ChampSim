#include <catch.hpp>

#include "repeatable.h"

namespace {
  struct mock_repeatable {
    bool ever_repeats;

    inline static int constructor_calls = 0;

    bool eof() const { return ever_repeats; }

    ooo_model_instr operator()() { return ooo_model_instr{0, input_instr{}}; }

    explicit mock_repeatable(bool r) : ever_repeats(r) { constructor_calls++; }
  };

  struct restart_indicator {
    bool* ext;
    void operator()() { *ext = true; }
  };
}

TEST_CASE("A repeatable repeats") {
  champsim::repeatable<mock_repeatable, bool> uut{true};

  auto old_calls = mock_repeatable::constructor_calls;
  (void)uut();
  REQUIRE(mock_repeatable::constructor_calls > old_calls);
}

namespace {
  template <typename T>
  struct configurable_repeatable {
    bool eof() const { return true; }

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
