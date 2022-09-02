#include "cache.h"

namespace
{
constexpr std::size_t REGION_COUNT = 128;
constexpr std::size_t MAX_DISTANCE = 256;
constexpr int PREFETCH_DEGREE = 2;

struct region_type {
  uint64_t vpn;
  uint64_t access_map;
  uint64_t prefetch_map;
  uint64_t lru;
};

std::array<region_type, REGION_COUNT> regions;
uint64_t region_lru;

int l2c_prefetch(CACHE* cache, uint64_t ip, uint64_t base_addr, uint64_t pf_addr, int pf_fill_level, int pf_metadata)
{
  if ((base_addr >> LOG2_BLOCK_SIZE) == (pf_addr >> LOG2_BLOCK_SIZE)) {
    // attempting to prefetch the same cache line!
    return -1;
  }

  // prefetching to different pages, so use virtual address prefetching
  if (cache->prefetch_line(pf_addr, pf_fill_level, pf_metadata)) {
    return 1;
  } else {
    return 0;
  }

  return 0;
}

void allocate_region(int region_index, uint64_t allocate_vpn)
{
  regions[region_index].vpn = allocate_vpn;
  regions[region_index].access_map = 0;
  regions[region_index].prefetch_map = 0;
  regions[region_index].lru = region_lru;
  region_lru++;
}

int find_region(uint64_t search_vpn)
{
  static int way_predict_index = 0;
  static uint64_t way_predict_vpn = 0;

  if (way_predict_vpn == search_vpn) {
    return way_predict_index;
  }

  int region_index = -1;
  for (std::size_t i = 0; i < REGION_COUNT; i++) {
    if (regions[i].vpn == search_vpn) {
      region_index = i;
      break;
    }
  }

  way_predict_index = region_index;
  way_predict_vpn = search_vpn;
  return region_index;
}

int get_lru_region()
{
  int lru_index = 0;
  uint64_t lru_value = regions[lru_index].lru;
  for (std::size_t i = 0; i < REGION_COUNT; i++) {
    if (regions[i].lru < lru_value) {
      lru_index = i;
      lru_value = regions[lru_index].lru;
    }
  }

  return lru_index;
}

bool check_access(int region_index, int region_offset) { return ((regions[region_index].access_map) >> region_offset) & 1; }

void set_access(int region_index, int region_offset)
{
  uint64_t one_set_bit = (1L << region_offset);
  regions[region_index].access_map |= one_set_bit;
}

/*
void reset_access(int region_index, int region_offset) { regions[region_index].access_map &= (~(1 << region_offset)); }
*/

bool check_prefetch(int region_index, int region_offset) { return ((regions[region_index].prefetch_map) >> region_offset) & 1; }

void set_prefetch(int region_index, int region_offset)
{
  uint64_t one_set_bit = (1L << region_offset);
  regions[region_index].prefetch_map |= one_set_bit;
}

/*
void reset_prefetch(int region_index, int region_offset) { regions[region_index].prefetch_map &= (~(1 << region_offset)); }
*/

bool check_cl_access(uint64_t v_addr)
{
  uint64_t vpn = v_addr >> LOG2_PAGE_SIZE;
  uint64_t page_offset = (v_addr >> LOG2_BLOCK_SIZE) & 63;
  int region_index = find_region(vpn);

  if (region_index == -1) {
    return false;
  }

  return check_access(region_index, page_offset);
}

void set_cl_access(uint64_t v_addr)
{
  uint64_t vpn = v_addr >> LOG2_PAGE_SIZE;
  uint64_t page_offset = (v_addr >> LOG2_BLOCK_SIZE) & 63;
  int region_index = find_region(vpn);

  if (region_index == -1) {
    // we're not currently tracking this region, so allocate a new region so we
    // can mark it
    int lru_index = get_lru_region();
    allocate_region(lru_index, vpn);
    region_index = lru_index;
  }

  set_access(region_index, page_offset);
}

/*
void reset_cl_access(uint64_t v_addr)
{
  uint64_t vpn = v_addr >> LOG2_PAGE_SIZE;
  uint64_t page_offset = (v_addr >> LOG2_BLOCK_SIZE) & 63;
  int region_index = find_region(vpn);

  if (region_index == -1) {
    // we're not currently tracking this region, but it doesn't matter so we
    // just do nothing
    return;
  }

  reset_access(region_index, page_offset);
}
*/

bool check_cl_prefetch(uint64_t v_addr)
{
  uint64_t vpn = v_addr >> LOG2_PAGE_SIZE;
  uint64_t page_offset = (v_addr >> LOG2_BLOCK_SIZE) & 63;
  int region_index = find_region(vpn);

  if (region_index == -1) {
    return false;
  }

  return check_prefetch(region_index, page_offset);
}

void set_cl_prefetch(uint64_t v_addr)
{
  uint64_t vpn = v_addr >> LOG2_PAGE_SIZE;
  uint64_t page_offset = (v_addr >> LOG2_BLOCK_SIZE) & 63;
  int region_index = find_region(vpn);

  if (region_index == -1) {
    // we're not currently tracking this region, so allocate a new region so we
    // can mark it
    int lru_index = get_lru_region();
    allocate_region(lru_index, vpn);
    region_index = lru_index;
  }

  set_prefetch(region_index, page_offset);
}

/*
void reset_cl_prefetch(uint64_t v_addr)
{
  uint64_t vpn = v_addr >> LOG2_PAGE_SIZE;
  uint64_t page_offset = (v_addr >> LOG2_BLOCK_SIZE) & 63;
  int region_index = find_region(vpn);

  if (region_index == -1) {
    // we're not currently tracking this region, but it doesn't matter so we
    // just do nothing
    return;
  }

  reset_prefetch(region_index, page_offset);
}
*/
} // anonymous namespace

void CACHE::prefetcher_initialize()
{
  std::cout << "CPU " << cpu << " Virtual Address Space AMPM-Lite Prefetcher" << std::endl;

  ::region_lru = 0;
  for (std::size_t i = 0; i < ::REGION_COUNT; i++) {
    ::allocate_region(i, 0);
  }
}

uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)
{
  uint64_t current_vpn = addr >> LOG2_PAGE_SIZE;
  int region_index = ::find_region(current_vpn);

  if (region_index == -1) {
    // not tracking this region yet, so replace the LRU region
    int lru_index = ::get_lru_region();
    ::allocate_region(lru_index, current_vpn);
    return metadata_in;
  }

  // mark this demand access
  ::set_cl_access(addr);

  // attempt to prefetch in the positive direction
  int prefetches_issued = 0;
  for (std::size_t i = 1; i <= MAX_DISTANCE; i++) {
    if ((::check_cl_access(addr - (i * BLOCK_SIZE))) && (::check_cl_access(addr - (2 * i * BLOCK_SIZE)))
        && (::check_cl_access(addr + (i * BLOCK_SIZE)) == false) && (::check_cl_prefetch(addr + (i * BLOCK_SIZE)) == false)) {
      // found something that we should prefetch
      int pf_fill_level = FILL_L2;
      if (get_occupancy(0, 0) > (get_size(0, 0) >> 1)) {
        pf_fill_level = FILL_LLC;
      }
      bool prefetch_success = (l2c_prefetch(this, ip, addr, addr + (i * BLOCK_SIZE), pf_fill_level, 0) > 0);
      if (prefetch_success) {
        ::set_cl_prefetch(addr + (i * BLOCK_SIZE));
        prefetches_issued++;
      }
    }

    if (prefetches_issued >= PREFETCH_DEGREE) {
      break;
    }
  }

  // attempt to prefetch in the negative direction
  prefetches_issued = 0;
  for (std::size_t i = 1; i <= MAX_DISTANCE; i++) {
    if ((::check_cl_access(addr + (i * BLOCK_SIZE))) && (::check_cl_access(addr + (2 * i * BLOCK_SIZE)))
        && (::check_cl_access(addr - (i * BLOCK_SIZE)) == false) && (::check_cl_prefetch(addr - (i * BLOCK_SIZE)) == false)) {
      // found something that we should prefetch
      int pf_fill_level = FILL_L2;
      if (get_occupancy(0, 0) > (get_size(0, 0) >> 1)) {
        pf_fill_level = FILL_LLC;
      }
      bool prefetch_success = (l2c_prefetch(this, ip, addr, addr - (i * BLOCK_SIZE), pf_fill_level, 0) > 0);
      if (prefetch_success) {
        ::set_cl_prefetch(addr - (i * BLOCK_SIZE));
        prefetches_issued++;
      }
    }

    if (prefetches_issued >= PREFETCH_DEGREE) {
      break;
    }
  }

  return metadata_in;
}

uint32_t CACHE::prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::prefetcher_cycle_operate() {}
void CACHE::prefetcher_final_stats() {}
