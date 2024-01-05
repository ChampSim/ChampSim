#include "va_ampm_lite.h"

#include <algorithm>

#include "cache.h"

uint64_t va_ampm_lite::region_type::region_lru = 0;

void va_ampm_lite::prefetcher_initialize() {}

auto va_ampm_lite::page_and_offset(uint64_t addr) -> std::pair<uint64_t, uint64_t>
{
  auto page_number = addr >> LOG2_PAGE_SIZE;
  auto page_offset = (addr & champsim::msl::bitmask(LOG2_PAGE_SIZE)) >> LOG2_BLOCK_SIZE;
  return {page_number, page_offset};
}

bool va_ampm_lite::check_cl_access(uint64_t v_addr)
{
  auto [vpn, page_offset] = page_and_offset(v_addr);
  auto region = std::find_if(std::begin(regions), std::end(regions), [vpn = vpn](auto x) { return x.vpn == vpn; });

  return (region != std::end(regions)) && region->access_map.at(page_offset);
}

bool va_ampm_lite::check_cl_prefetch(uint64_t v_addr)
{
  auto [vpn, page_offset] = page_and_offset(v_addr);
  auto region = std::find_if(std::begin(regions), std::end(regions), [vpn = vpn](auto x) { return x.vpn == vpn; });

  return (region != std::end(regions)) && region->prefetch_map.at(page_offset);
}

uint32_t va_ampm_lite::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, access_type type, uint32_t metadata_in)
{
  auto [current_vpn, page_offset] = page_and_offset(addr);
  auto demand_region = std::find_if(std::begin(regions), std::end(regions), [vpn = current_vpn](auto x) { return x.vpn == vpn; });

  if (demand_region == std::end(regions)) {
    // not tracking this region yet, so replace the LRU region
    demand_region = std::min_element(std::begin(regions), std::end(regions), [](auto x, auto y) { return x.lru < y.lru; });
    *demand_region = region_type{current_vpn};
    return metadata_in;
  }

  // mark this demand access
  demand_region->access_map.at(page_offset) = true;

  // attempt to prefetch in the positive, then negative direction
  for (auto direction : {1, -1}) {
    for (int i = 1, prefetches_issued = 0; i <= MAX_DISTANCE && prefetches_issued < PREFETCH_DEGREE; i++) {
      const auto pos_step_addr = addr + direction * (i * (signed)BLOCK_SIZE);
      const auto neg_step_addr = addr - direction * (i * (signed)BLOCK_SIZE);
      const auto neg_2step_addr = addr - direction * (2 * i * (signed)BLOCK_SIZE);

      if (check_cl_access(neg_step_addr) && check_cl_access(neg_2step_addr) && !check_cl_access(pos_step_addr) && !check_cl_prefetch(pos_step_addr)) {
        // found something that we should prefetch
        if ((addr >> LOG2_BLOCK_SIZE) != (pos_step_addr >> LOG2_BLOCK_SIZE)) {
          bool prefetch_success = prefetch_line(pos_step_addr, (intern_->get_mshr_occupancy_ratio() < 0.5), metadata_in);
          if (prefetch_success) {
            auto [pf_vpn, pf_page_offset] = page_and_offset(pos_step_addr);
            auto pf_region = std::find_if(std::begin(regions), std::end(regions), [vpn = pf_vpn](auto x) { return x.vpn == vpn; });

            if (pf_region == std::end(regions)) {
              // we're not currently tracking this region, so allocate a new region so we can mark it
              pf_region = std::min_element(std::begin(regions), std::end(regions), [](auto x, auto y) { return x.lru < y.lru; });
              *pf_region = region_type{pf_vpn};
            }

            pf_region->prefetch_map.at(pf_page_offset) = true;
            prefetches_issued++;
          }
        }
      }
    }
  }

  return metadata_in;
}

uint32_t va_ampm_lite::prefetcher_cache_fill(uint64_t addr, long set, long way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}
