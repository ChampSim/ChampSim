#include <functional>

#include "cache.h"
#include "ooo_cpu.h"

namespace
{
struct take_last {
  template <typename T, typename U>
  U operator()(T&&, U&& last) const
  {
    return std::forward<U>(last);
  }
};
} // namespace

#include "module_defs.inc"
