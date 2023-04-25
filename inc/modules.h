#ifndef MODULES_H
#define MODULES_H

class CACHE;
class O3_CPU;

namespace champsim::modules
{
inline constexpr bool warn_if_any_missing = true;
template <typename T>
[[deprecated]] void does_not_have()
{
}

struct branch_predictor {
  O3_CPU* intern_;
  explicit branch_predictor(O3_CPU* cpu) : intern_(cpu) {}
};

struct btb {
  O3_CPU* intern_;
  explicit btb(O3_CPU* cpu) : intern_(cpu) {}
};

struct prefetcher {
  CACHE* intern_;
  explicit prefetcher(CACHE* cache) : intern_(cache) {}
};

struct replacement {
  CACHE* intern_;
  explicit replacement(CACHE* cache) : intern_(cache) {}
};
} // namespace champsim::modules

#include "cache.h"
#include "ooo_cpu.h"

#endif
