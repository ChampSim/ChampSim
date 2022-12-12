#include <bitset>
#include <map>
#include <vector>

#include "cache.h"

namespace
{
constexpr std::size_t REGION_COUNT = 128;
constexpr std::size_t MAX_DISTANCE = 256;
constexpr int PREFETCH_DEGREE = 2;

using block_in_page = champsim::address_slice<LOG2_PAGE_SIZE, LOG2_BLOCK_SIZE>;

template <typename T>
auto page_and_block(T addr)
{
  return std::pair{champsim::page_number{addr}, block_in_page{addr}};
}

struct region_type {
  champsim::page_number vpn{};
  std::bitset<PAGE_SIZE / BLOCK_SIZE> access_map{};
  std::bitset<PAGE_SIZE / BLOCK_SIZE> prefetch_map{};
  uint64_t lru;

  static uint64_t region_lru;

  region_type() : region_type(champsim::page_number{}) {}
  explicit region_type(champsim::page_number allocate_vpn) : vpn(allocate_vpn), lru(region_lru++) {}
};
uint64_t region_type::region_lru = 0;

std::map<CACHE*, std::array<region_type, REGION_COUNT>> regions;

bool check_cl_access(CACHE* cache, champsim::block_number v_addr)
{
  auto [vpn, page_offset] = page_and_block(v_addr);
  auto region = std::find_if(std::begin(regions.at(cache)), std::end(regions.at(cache)), [vpn](auto x) { return x.vpn == vpn; });

  return (region != std::end(regions.at(cache))) && region->prefetch_map.test(page_offset.to<uint64_t>());
}

bool check_cl_prefetch(CACHE* cache, champsim::block_number v_addr)
{
  auto [vpn, page_offset] = page_and_block(v_addr);
  auto region = std::find_if(std::begin(regions.at(cache)), std::end(regions.at(cache)), [vpn](auto x) { return x.vpn == vpn; });

  return (region != std::end(regions.at(cache))) && region->prefetch_map.test(page_offset.to<uint64_t>());
}

} // anonymous namespace

void CACHE::prefetcher_initialize() { std::cout << "CPU " << cpu << " Virtual Address Space AMPM-Lite Prefetcher" << std::endl; }

uint32_t CACHE::prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)
{
  const champsim::block_number block_addr{addr};
  auto [current_vpn, page_offset] = page_and_block(block_addr);
  auto demand_region = std::find_if(std::begin(::regions.at(this)), std::end(::regions.at(this)), [current_vpn](auto x) { return x.vpn == current_vpn; });

  if (demand_region == std::end(::regions.at(this))) {
    // not tracking this region yet, so replace the LRU region
    demand_region = std::min_element(std::begin(::regions.at(this)), std::end(::regions.at(this)), [](auto x, auto y) { return x.lru < y.lru; });
    *demand_region = region_type{current_vpn};
    return metadata_in;
  }

  // mark this demand access
  demand_region->access_map.set(page_offset.to<std::size_t>());

  // attempt to prefetch in the positive, then negative direction
  for (auto direction : {1, -1}) {
    for (std::size_t i = 1, prefetches_issued = 0; i <= MAX_DISTANCE && prefetches_issued < PREFETCH_DEGREE; i++) {
      const auto pos_step_addr = block_addr + direction * i;
      const auto neg_step_addr = block_addr - direction * i;
      const auto neg_2step_addr = block_addr - direction * 2 * i;
      if (::check_cl_access(this, neg_step_addr) && ::check_cl_access(this, neg_2step_addr) && !::check_cl_access(this, pos_step_addr)
          && !::check_cl_prefetch(this, pos_step_addr)) {
        // found something that we should prefetch
        if (block_addr != champsim::block_number{pos_step_addr}) {
          champsim::address pf_addr{pos_step_addr};
          bool prefetch_success = prefetch_line(pf_addr, get_occupancy(0, pf_addr) < get_size(0, pf_addr) / 2, metadata_in);
          if (prefetch_success) {
            auto [pf_vpn, pf_page_offset] = page_and_block(pf_addr);
            auto pf_region = std::find_if(std::begin(::regions.at(this)), std::end(::regions.at(this)), [pf_vpn](auto x) { return x.vpn == pf_vpn; });

            if (pf_region == std::end(::regions.at(this))) {
              // we're not currently tracking this region, so allocate a new region so we can mark it
              pf_region = std::min_element(std::begin(::regions.at(this)), std::end(::regions.at(this)), [](auto x, auto y) { return x.lru < y.lru; });
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

uint32_t CACHE::prefetcher_cache_fill(champsim::address addr, uint32_t set, uint32_t way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}
void CACHE::prefetcher_final_stats() {}
