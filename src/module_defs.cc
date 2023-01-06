#include "ooo_cpu.h"
#include "cache.h"

#include <functional>

namespace {
    struct take_last
    {
      template <typename T, typename U>
      U operator()(T&&, U&& last) const {
        return std::forward<U>(last);
      }
    };
}

#include "module_defs.inc"

