#include <bitset>
#include <map>
#include <vector>

#include "cache.h"

namespace
{
constexpr std::size_t REGION_COUNT = 128;
constexpr std::size_t MAX_DISTANCE = 256;
constexpr int PREFETCH_DEGREE = 2;

struct region_type {
  champsim::page_number vpn{};
  std::bitset<PAGE_SIZE / BLOCK_SIZE> access_map{};
  std::bitset<PAGE_SIZE / BLOCK_SIZE> prefetch_map{};
  uint64_t lru;

  static uint64_t region_lru;

  region_type() = default;
  explicit region_type(champsim::page_number allocate_vpn) : vpn(allocate_vpn), lru(region_lru++) {}
};
uint64_t region_type::region_lru = 0;

std::map<CACHE*, std::array<region_type, REGION_COUNT>> regions;

bool check_cl_access(CACHE* cache, champsim::address v_addr)
{
  champsim::page_number vpn{v_addr};
  auto page_offset = champsim::address_slice<LOG2_PAGE_SIZE, LOG2_BLOCK_SIZE>{v_addr}.to<uint64_t>();
  auto region = std::find_if(std::begin(regions.at(cache)), std::end(regions.at(cache)), [vpn](auto x) { return x.vpn == vpn; });

  return (region != std::end(regions.at(cache))) && region->prefetch_map.test(page_offset);
}

bool check_cl_prefetch(CACHE* cache, champsim::address v_addr)
{
  champsim::page_number vpn{v_addr};
  auto page_offset = champsim::address_slice<LOG2_PAGE_SIZE, LOG2_BLOCK_SIZE>{v_addr}.to<uint64_t>();
  auto region = std::find_if(std::begin(regions.at(cache)), std::end(regions.at(cache)), [vpn](auto x) { return x.vpn == vpn; });

  return (region != std::end(regions.at(cache))) && region->prefetch_map.test(page_offset);
}

} // anonymous namespace

void CACHE::prefetcher_initialize() { std::cout << "CPU " << cpu << " Virtual Address Space AMPM-Lite Prefetcher" << std::endl; }

uint32_t CACHE::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)
{
  champsim::page_number current_vpn{addr};
  auto page_offset = champsim::address_slice<LOG2_PAGE_SIZE, LOG2_BLOCK_SIZE>{addr}.to<uint64_t>();
  auto demand_region = std::find_if(std::begin(::regions.at(this)), std::end(::regions.at(this)), [current_vpn](auto x) { return x.vpn == current_vpn; });

  if (demand_region == std::end(::regions.at(this))) {
    // not tracking this region yet, so replace the LRU region
    demand_region = std::min_element(std::begin(::regions.at(this)), std::end(::regions.at(this)), [](auto x, auto y) { return x.lru < y.lru; });
    *demand_region = region_type{current_vpn};
    return metadata_in;
  }

  // mark this demand access
  demand_region->access_map.set(page_offset);

  // attempt to prefetch in the positive, then negative direction
  for (auto direction : {1, -1}) {
    for (std::size_t i = 1, prefetches_issued = 0; i <= MAX_DISTANCE && prefetches_issued < PREFETCH_DEGREE; i++) {
      const auto pos_step_addr = addr + direction * (i * BLOCK_SIZE);
      const auto neg_step_addr = addr - direction * (i * BLOCK_SIZE);
      const auto neg_2step_addr = addr - direction * (2 * i * BLOCK_SIZE);
      if (::check_cl_access(this, neg_step_addr) && ::check_cl_access(this, neg_2step_addr) && !::check_cl_access(this, pos_step_addr)
          && !::check_cl_prefetch(this, pos_step_addr)) {
        // found something that we should prefetch
        if (champsim::block_number{addr} != champsim::block_number{pos_step_addr}) {
          bool prefetch_success = prefetch_line(pos_step_addr, get_occupancy(0, pos_step_addr) < get_size(0, pos_step_addr) / 2, metadata_in);
          if (prefetch_success) {
            champsim::page_number pf_vpn{pos_step_addr};
            auto pf_page_offset = champsim::address_slice<LOG2_PAGE_SIZE, LOG2_BLOCK_SIZE>{pos_step_addr}.to<uint64_t>();
            auto pf_region = std::find_if(std::begin(::regions.at(this)), std::end(::regions.at(this)), [pf_vpn](auto x) { return x.vpn == pf_vpn; });

            if (pf_region == std::end(::regions.at(this))) {
              // we're not currently tracking this region, so allocate a new region so we can mark it
              pf_region = std::min_element(std::begin(::regions.at(this)), std::end(::regions.at(this)), [](auto x, auto y) { return x.lru < y.lru; });
              *pf_region = region_type{pf_vpn};
            }

            pf_region->prefetch_map.set(pf_page_offset);
            prefetches_issued++;
          }
        }
      }
    }
  }

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(champsim::address addr, uint32_t set, uint32_t way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}
void CACHE::prefetcher_final_stats() {}
