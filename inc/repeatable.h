#ifndef REPEATABLE_H
#define REPEATABLE_H

#include <memory>
#include <string>

#include <fmt/ranges.h>

#include "instruction.h"

namespace champsim
{
template <typename T, typename... Args>
struct repeatable {
  static_assert(std::is_move_constructible_v<T>);
  static_assert(std::is_move_assignable_v<T>);
  std::tuple<Args...> args_;
  T intern_{std::apply([](auto... x) { return T{x...}; }, args_)};
  explicit repeatable(Args... args) : args_(args...) {}

  auto operator()()
  {
    // Reopen trace if we've reached the end of the file
    if (intern_.eof()) {
      fmt::print("*** Reached end of trace: {}\n", args_);
      intern_ = T{std::apply([](auto... x) { return T{x...}; }, args_)};
    }

    return intern_();
  }
};
} // namespace champsim

#endif
