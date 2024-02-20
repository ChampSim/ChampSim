// NOLINTBEGIN(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers): generated magic numbers

#include "environment.h"
#include "core_inst.inc"

#if __has_include("module_def.inc")
#include "module_def.inc"
#endif

#include "defaults.hpp"
#include "vmem.h"
#include "chrono.h"

namespace champsim::configured {
  template <typename R, typename... PTWs>
  auto build(PTWs... builders) {
    std::vector<R> retval{};
    (retval.emplace_back(builders), ...);
    return retval;
  }

#include "core_inst.cc.inc"
}

// NOLINTEND(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers)
