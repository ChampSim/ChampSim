// NOLINTBEGIN(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers): generated magic numbers

#include "core_inst.inc"
#include "environment.h"

#if __has_include("legacy_bridge.h")
#include "legacy_bridge.h"
#endif

#include "chrono.h"
#include "defaults.hpp"
#include "vmem.h"

namespace champsim::configured
{
template <typename R, typename... PTWs>
auto build(PTWs... builders)
{
  std::vector<R> retval{};
  (retval.emplace_back(builders), ...);
  return retval;
}
} // namespace champsim::configured

#if __has_include("core_inst.cc.inc")
#include "core_inst.cc.inc"
#endif

// NOLINTEND(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers)
