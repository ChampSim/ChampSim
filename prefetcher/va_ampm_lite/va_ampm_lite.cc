#include "cache.h"

#define L2C_VA_AMPM_LITE_REGION_COUNT 128
#define L2C_VA_AMPM_LITE_MAX_DISTANCE 256
#define L2C_VA_AMPM_LITE_PREFETCH_DEGREE 2

struct l2c_va_ampm_lite_region_t {
  uint64_t vpn;
  uint64_t access_map;
  uint64_t prefetch_map;
  uint64_t lru;
};

l2c_va_ampm_lite_region_t l2c_va_ampm_lite_regions[L2C_VA_AMPM_LITE_REGION_COUNT];
uint64_t l2c_va_ampm_lite_region_lru;

int l2c_prefetch(CACHE* cache, uint64_t ip, uint64_t base_addr, uint64_t pf_addr, int pf_fill_level, int pf_metadata)
{
  if ((base_addr >> LOG2_BLOCK_SIZE) == (pf_addr >> LOG2_BLOCK_SIZE)) {
    // attempting to prefetch the same cache line!
    return -1;
  }

  // prefetching to different pages, so use virtual address prefetching
  if (cache->prefetch_line(ip, base_addr, pf_addr, pf_fill_level, pf_metadata)) {
    return 1;
  } else {
    return 0;
  }

  return 0;
}

void va_ampm_allocate_region(int region_index, uint64_t allocate_vpn)
{
  l2c_va_ampm_lite_regions[region_index].vpn = allocate_vpn;
  l2c_va_ampm_lite_regions[region_index].access_map = 0;
  l2c_va_ampm_lite_regions[region_index].prefetch_map = 0;
  l2c_va_ampm_lite_regions[region_index].lru = l2c_va_ampm_lite_region_lru;
  l2c_va_ampm_lite_region_lru++;
}

int va_ampm_find_region(uint64_t search_vpn)
{
  static int way_predict_index = 0;
  static uint64_t way_predict_vpn = 0;

  if (way_predict_vpn == search_vpn) {
    return way_predict_index;
  }

  int region_index = -1;
  for (int i = 0; i < L2C_VA_AMPM_LITE_REGION_COUNT; i++) {
    if (l2c_va_ampm_lite_regions[i].vpn == search_vpn) {
      region_index = i;
      break;
    }
  }

  way_predict_index = region_index;
  way_predict_vpn = search_vpn;
  return region_index;
}

int va_ampm_get_lru_region()
{
  int lru_index = 0;
  uint64_t lru_value = l2c_va_ampm_lite_regions[lru_index].lru;
  for (int i = 0; i < L2C_VA_AMPM_LITE_REGION_COUNT; i++) {
    if (l2c_va_ampm_lite_regions[i].lru < lru_value) {
      lru_index = i;
      lru_value = l2c_va_ampm_lite_regions[lru_index].lru;
    }
  }

  return lru_index;
}

bool va_ampm_check_access(int region_index, int region_offset) { return ((l2c_va_ampm_lite_regions[region_index].access_map) >> region_offset) & 1; }

void va_ampm_set_access(int region_index, int region_offset)
{
  uint64_t one_set_bit = (1L << region_offset);
  l2c_va_ampm_lite_regions[region_index].access_map |= one_set_bit;
}

void va_ampm_reset_access(int region_index, int region_offset) { l2c_va_ampm_lite_regions[region_index].access_map &= (~(1 << region_offset)); }

bool va_ampm_check_prefetch(int region_index, int region_offset) { return ((l2c_va_ampm_lite_regions[region_index].prefetch_map) >> region_offset) & 1; }

void va_ampm_set_prefetch(int region_index, int region_offset)
{
  uint64_t one_set_bit = (1L << region_offset);
  l2c_va_ampm_lite_regions[region_index].prefetch_map |= one_set_bit;
}

void va_ampm_reset_prefetch(int region_index, int region_offset) { l2c_va_ampm_lite_regions[region_index].prefetch_map &= (~(1 << region_offset)); }

bool va_ampm_check_cl_access(uint64_t v_addr)
{
  uint64_t vpn = v_addr >> LOG2_PAGE_SIZE;
  uint64_t page_offset = (v_addr >> LOG2_BLOCK_SIZE) & 63;
  int region_index = va_ampm_find_region(vpn);

  if (region_index == -1) {
    return false;
  }

  return va_ampm_check_access(region_index, page_offset);
}

void va_ampm_set_cl_access(uint64_t v_addr)
{
  uint64_t vpn = v_addr >> LOG2_PAGE_SIZE;
  uint64_t page_offset = (v_addr >> LOG2_BLOCK_SIZE) & 63;
  int region_index = va_ampm_find_region(vpn);

  if (region_index == -1) {
    // we're not currently tracking this region, so allocate a new region so we
    // can mark it
    int lru_index = va_ampm_get_lru_region();
    va_ampm_allocate_region(lru_index, vpn);
    region_index = lru_index;
  }

  va_ampm_set_access(region_index, page_offset);
}

void va_ampm_reset_cl_access(uint64_t v_addr)
{
  uint64_t vpn = v_addr >> LOG2_PAGE_SIZE;
  uint64_t page_offset = (v_addr >> LOG2_BLOCK_SIZE) & 63;
  int region_index = va_ampm_find_region(vpn);

  if (region_index == -1) {
    // we're not currently tracking this region, but it doesn't matter so we
    // just do nothing
    return;
  }

  va_ampm_reset_access(region_index, page_offset);
}

bool va_ampm_check_cl_prefetch(uint64_t v_addr)
{
  uint64_t vpn = v_addr >> LOG2_PAGE_SIZE;
  uint64_t page_offset = (v_addr >> LOG2_BLOCK_SIZE) & 63;
  int region_index = va_ampm_find_region(vpn);

  if (region_index == -1) {
    return false;
  }

  return va_ampm_check_prefetch(region_index, page_offset);
}

void va_ampm_set_cl_prefetch(uint64_t v_addr)
{
  uint64_t vpn = v_addr >> LOG2_PAGE_SIZE;
  uint64_t page_offset = (v_addr >> LOG2_BLOCK_SIZE) & 63;
  int region_index = va_ampm_find_region(vpn);

  if (region_index == -1) {
    // we're not currently tracking this region, so allocate a new region so we
    // can mark it
    int lru_index = va_ampm_get_lru_region();
    va_ampm_allocate_region(lru_index, vpn);
    region_index = lru_index;
  }

  va_ampm_set_prefetch(region_index, page_offset);
}

void va_ampm_reset_cl_prefetch(uint64_t v_addr)
{
  uint64_t vpn = v_addr >> LOG2_PAGE_SIZE;
  uint64_t page_offset = (v_addr >> LOG2_BLOCK_SIZE) & 63;
  int region_index = va_ampm_find_region(vpn);

  if (region_index == -1) {
    // we're not currently tracking this region, but it doesn't matter so we
    // just do nothing
    return;
  }

  va_ampm_reset_prefetch(region_index, page_offset);
}

void CACHE::l2c_prefetcher_initialize()
{
  cout << "CPU " << cpu << " L2C Virtual Address Space AMPM-Lite Prefetcher" << endl;

  l2c_va_ampm_lite_region_lru = 0;
  for (int i = 0; i < L2C_VA_AMPM_LITE_REGION_COUNT; i++) {
    va_ampm_allocate_region(i, 0);
  }
}

uint32_t CACHE::l2c_prefetcher_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)
{
  uint64_t current_vpn = addr >> LOG2_PAGE_SIZE;
  int region_index = va_ampm_find_region(current_vpn);

  if (region_index == -1) {
    // not tracking this region yet, so replace the LRU region
    int lru_index = va_ampm_get_lru_region();
    va_ampm_allocate_region(lru_index, current_vpn);
    return metadata_in;
  }

  // mark this demand access
  va_ampm_set_cl_access(addr);

  // attempt to prefetch in the positive direction
  int prefetches_issued = 0;
  for (int i = 1; i <= L2C_VA_AMPM_LITE_MAX_DISTANCE; i++) {
    if ((va_ampm_check_cl_access(addr - (i * BLOCK_SIZE))) && (va_ampm_check_cl_access(addr - (2 * i * BLOCK_SIZE)))
        && (va_ampm_check_cl_access(addr + (i * BLOCK_SIZE)) == false) && (va_ampm_check_cl_prefetch(addr + (i * BLOCK_SIZE)) == false)) {
      // found something that we should prefetch
      int pf_fill_level = FILL_L2;
      if (get_occupancy(0, 0) > (get_size(0, 0) >> 1)) {
        pf_fill_level = FILL_LLC;
      }
      bool prefetch_success = (l2c_prefetch(this, ip, addr, addr + (i * BLOCK_SIZE), pf_fill_level, 0) > 0);
      if (prefetch_success) {
        va_ampm_set_cl_prefetch(addr + (i * BLOCK_SIZE));
        prefetches_issued++;
      }
    }

    if (prefetches_issued >= L2C_VA_AMPM_LITE_PREFETCH_DEGREE) {
      break;
    }
  }

  // attempt to prefetch in the negative direction
  prefetches_issued = 0;
  for (int i = 1; i <= L2C_VA_AMPM_LITE_MAX_DISTANCE; i++) {
    if ((va_ampm_check_cl_access(addr + (i * BLOCK_SIZE))) && (va_ampm_check_cl_access(addr + (2 * i * BLOCK_SIZE)))
        && (va_ampm_check_cl_access(addr - (i * BLOCK_SIZE)) == false) && (va_ampm_check_cl_prefetch(addr - (i * BLOCK_SIZE)) == false)) {
      // found something that we should prefetch
      int pf_fill_level = FILL_L2;
      if (get_occupancy(0, 0) > (get_size(0, 0) >> 1)) {
        pf_fill_level = FILL_LLC;
      }
      bool prefetch_success = (l2c_prefetch(this, ip, addr, addr - (i * BLOCK_SIZE), pf_fill_level, 0) > 0);
      if (prefetch_success) {
        va_ampm_set_cl_prefetch(addr - (i * BLOCK_SIZE));
        prefetches_issued++;
      }
    }

    if (prefetches_issued >= L2C_VA_AMPM_LITE_PREFETCH_DEGREE) {
      break;
    }
  }

  return metadata_in;
}

uint32_t CACHE::l2c_prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::l2c_prefetcher_final_stats() {}
