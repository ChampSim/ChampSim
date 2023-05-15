#ifndef REPEATABLE_H
#define REPEATABLE_H

#include <iostream>
#include <memory>
#include <string>

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
      std::cout << "*** Reached end of trace: { ";
      std::apply([&](auto... x) { (..., (std::cout << x << ", ")); }, args_);
      std::cout << "\b\b }" << std::endl;
      intern_ = T{std::apply([](auto... x) { return T{x...}; }, args_)};
    }

    return intern_();
  }

  bool eof() const { return false; }
};
} // namespace champsim

#endif
