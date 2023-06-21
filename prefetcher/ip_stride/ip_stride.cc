#include "ip_stride.h"
#include "cache.h"

uint32_t ip_stride::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, access_type type, uint32_t metadata_in)
{
  uint64_t cl_addr = addr >> LOG2_BLOCK_SIZE;
  int64_t stride = 0;

  auto found = table.check_hit({ip, cl_addr, stride});

  // if we found a matching entry
  if (found.has_value()) {
    // calculate the stride between the current address and the last address
    // no need to check for overflow since these values are downshifted
    stride = static_cast<int64_t>(cl_addr) - static_cast<int64_t>(found->last_cl_addr);

    // Initialize prefetch state unless we somehow saw the same address twice in
    // a row or if this is the first time we've seen this stride
    if (stride != 0 && stride == found->last_stride)
      active_lookahead = {cl_addr << LOG2_BLOCK_SIZE, stride, PREFETCH_DEGREE};
  }

  // update tracking set
  table.fill({ip, cl_addr, stride});

  return metadata_in;
}

void ip_stride::prefetcher_cycle_operate()
{
  // If a lookahead is active
  if (active_lookahead.has_value()) {
    auto [old_pf_address, stride, degree] = active_lookahead.value();
    assert(degree > 0);

    auto addr_delta = stride * BLOCK_SIZE;
    auto pf_address = static_cast<uint64_t>(static_cast<int64_t>(old_pf_address) + addr_delta); // cast to signed to allow negative strides

    // If the next step would exceed the degree or run off the page, stop
    if (intern_->virtual_prefetch || (pf_address >> LOG2_PAGE_SIZE) == (old_pf_address >> LOG2_PAGE_SIZE)) {
      // check the MSHR occupancy to decide if we're going to prefetch to this level or not
      bool success = prefetch_line(pf_address, (intern_->get_mshr_occupancy_ratio() < 0.5), 0);
      if (success)
        active_lookahead = {pf_address, stride, degree - 1};
      // If we fail, try again next cycle

      if (active_lookahead->degree == 0) {
        active_lookahead.reset();
      }
    } else {
      active_lookahead.reset();
    }
  }
}

uint32_t ip_stride::prefetcher_cache_fill(uint64_t addr, long set, long way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}
