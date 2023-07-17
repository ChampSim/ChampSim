#include "va_ampm_lite.h"

#include <algorithm>
#include "cache.h"

uint64_t va_ampm_lite::region_type::region_lru = 0;

void va_ampm_lite::prefetcher_initialize() {}

bool va_ampm_lite::check_cl_access(champsim::block_number v_addr)
{
  auto [vpn, page_offset] = page_and_offset(v_addr);
  auto region = std::find_if(std::begin(regions), std::end(regions), [vpn = vpn](auto x) { return x.vpn == vpn; });

  return (region != std::end(regions)) && region->access_map.test(page_offset.to<std::size_t>());
}

bool va_ampm_lite::check_cl_prefetch(champsim::block_number v_addr)
{
  auto [vpn, page_offset] = page_and_offset(v_addr);
  auto region = std::find_if(std::begin(regions), std::end(regions), [vpn = vpn](auto x) { return x.vpn == vpn; });

  return (region != std::end(regions)) && region->prefetch_map.test(page_offset.to<std::size_t>());
}

uint32_t va_ampm_lite::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type, uint32_t metadata_in)
{
  auto [current_vpn, page_offset] = page_and_offset(addr);
  champsim::block_number block_addr{addr};
  auto demand_region = std::find_if(std::begin(regions), std::end(regions), [vpn = current_vpn](auto x) { return x.vpn == vpn; });

  if (demand_region == std::end(regions)) {
    // not tracking this region yet, so replace the LRU region
    demand_region = std::min_element(std::begin(regions), std::end(regions), [](auto x, auto y) { return x.lru < y.lru; });
    *demand_region = region_type{current_vpn};
    return metadata_in;
  }

  // mark this demand access
  demand_region->access_map.set(page_offset.to<std::size_t>());

  // attempt to prefetch in the positive, then negative direction
  for (auto direction : {1, -1}) {
    for (int i = 1, prefetches_issued = 0; i <= MAX_DISTANCE && prefetches_issued < PREFETCH_DEGREE; i++) {
      const auto pos_step_addr = block_addr + (direction * i);
      const auto neg_step_addr = block_addr - (direction * i);
      const auto neg_2step_addr = block_addr - (direction * 2 * i);

      if (check_cl_access(neg_step_addr) && check_cl_access(neg_2step_addr) && !check_cl_access(pos_step_addr) && !check_cl_prefetch(pos_step_addr)) {
        // found something that we should prefetch
        if (block_addr != champsim::block_number{pos_step_addr}) {
          champsim::address pf_addr{pos_step_addr};
          if (bool prefetch_success = prefetch_line(pf_addr, (intern_->get_mshr_occupancy_ratio() < 0.5), metadata_in); prefetch_success) {
            auto [pf_vpn, pf_page_offset] = page_and_offset(pf_addr);
            auto pf_region = std::find_if(std::begin(regions), std::end(regions), [vpn = pf_vpn](auto x) { return x.vpn == vpn; });

            if (pf_region == std::end(regions)) {
              // we're not currently tracking this region, so allocate a new region so we can mark it
              pf_region = std::min_element(std::begin(regions), std::end(regions), [](auto x, auto y) { return x.lru < y.lru; });
              *pf_region = region_type{pf_vpn};
            }

            pf_region->prefetch_map.set(pf_page_offset.to<std::size_t>());
            prefetches_issued++;
          }
        }
      }
    }
  }

  return metadata_in;
}

uint32_t va_ampm_lite::prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}
