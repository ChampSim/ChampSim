#ifndef REPEATABLE_H
#define REPEATABLE_H

#include <memory>
#include <iostream>
#include <string>

#include "instruction.h"

namespace champsim
{
template <typename T>
struct repeatable : public T
{
  template <typename... Args>
  explicit repeatable(Args... args) : T(std::forward<Args>(args)...) {}

  auto operator()()
  {
    // Reopen trace if we've reached the end of the file
    if (T::eof()) {
      auto name = T::trace_string;
      std::cout << "*** Reached end of trace: " << name << std::endl;
      T::restart();
    }

    return T::operator()();
  }
};
} // namespace champsim

#endif
