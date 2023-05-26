#include "modules.h"

#include "cache.h"

uint32_t champsim::modules::prefetcher::prefetch_line(champsim::address pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  return intern_->prefetch_line(pf_addr, fill_this_level, prefetch_metadata);
}

// LCOV_EXCL_START Exclude deprecated function
uint32_t champsim::modules::prefetcher::prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  return prefetch_line(champsim::address{pf_addr}, fill_this_level, prefetch_metadata);
}
// LCOV_EXCL_STOP
