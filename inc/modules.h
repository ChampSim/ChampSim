#ifndef MODULES_H
#define MODULES_H

#include "cache.h"
#include "ooo_cpu.h"

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
  auto prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
  {
    return intern_->prefetch_line(pf_addr, fill_this_level, prefetch_metadata);
  }
};

struct replacement {
  CACHE* intern_;
  explicit replacement(CACHE* cache) : intern_(cache) {}
};
} // namespace champsim::modules

#endif
