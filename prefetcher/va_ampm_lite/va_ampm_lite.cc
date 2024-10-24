#include "va_ampm_lite.h"

#include <algorithm>

#include "cache.h"

template <typename T>
auto va_ampm_lite::page_and_offset(T addr) -> std::pair<champsim::page_number, block_in_page>
{
  return std::pair{champsim::page_number{addr}, block_in_page{addr}};
}

bool va_ampm_lite::check_cl_access(champsim::block_number v_addr)
{
  auto [vpn, page_offset] = page_and_offset(v_addr);
  auto region = regions.check_hit(region_type{vpn});

  return (region.has_value() && region->access_map.at(page_offset.to<std::size_t>()));
}

bool va_ampm_lite::check_cl_prefetch(champsim::block_number v_addr)
{
  auto [vpn, page_offset] = page_and_offset(v_addr);
  auto region = regions.check_hit(region_type{vpn});

  return (region.has_value() && region->prefetch_map.at(page_offset.to<std::size_t>()));
}

uint32_t va_ampm_lite::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type,
                                                uint32_t metadata_in)
{
  auto [current_vpn, page_offset] = page_and_offset(addr);
  champsim::block_number block_addr{addr};
  auto demand_region = regions.check_hit(region_type{current_vpn});

  if (!demand_region.has_value()) {
    // not tracking this region yet, so replace the LRU region
    regions.fill(region_type{current_vpn});
    return metadata_in;
  }
  // mark this demand access
  demand_region->access_map.at(page_offset.to<std::size_t>()) = true;
  regions.fill(demand_region.value());

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
            auto [pf_vpn, pf_page_offset] = page_and_offset(pos_step_addr);
            auto pf_region = regions.check_hit(region_type{pf_vpn});

            if (!pf_region.has_value()) {
              // we're not currently tracking this region, so allocate a new region so we can mark it
              region_type new_region{pf_vpn};
              new_region.prefetch_map.at(pf_page_offset.to<std::size_t>()) = true;
              regions.fill(new_region);
            } else {
              pf_region.value().prefetch_map.at(pf_page_offset.to<std::size_t>()) = true;
              regions.fill(pf_region.value());
            }
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
