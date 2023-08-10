#ifndef PREFETCHER_VA_AMPM_LITE_H
#define PREFETCHER_VA_AMPM_LITE_H

#include <array>
#include <bitset>
#include <cstdint>

#include "champsim.h"
#include "modules.h"

#include "champsim_constants.h"

struct va_ampm_lite : champsim::modules::prefetcher
{
  static constexpr std::size_t REGION_COUNT = 128;
  static constexpr int MAX_DISTANCE = 256;
  static constexpr int PREFETCH_DEGREE = 2;

  using block_in_page = champsim::address_slice<LOG2_PAGE_SIZE, LOG2_BLOCK_SIZE>;

  struct region_type {
    champsim::page_number vpn;
    std::bitset<PAGE_SIZE / BLOCK_SIZE> access_map{};
    std::bitset<PAGE_SIZE / BLOCK_SIZE> prefetch_map{};
    uint64_t lru;

    static uint64_t region_lru;

    region_type() : region_type(champsim::page_number{}) {}
    explicit region_type(champsim::page_number allocate_vpn) : vpn(allocate_vpn), lru(region_lru++) {}
  };

  using prefetcher::prefetcher;

  std::array<region_type, REGION_COUNT> regions;

  bool check_cl_access(champsim::block_number v_addr);
  bool check_cl_prefetch(champsim::block_number v_addr);

  template <typename T>
  static auto page_and_offset(T addr) -> std::pair<champsim::page_number, block_in_page>;

  uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, bool useful_prefetch, access_type type, uint32_t metadata_in);
  uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in);

  //void prefetcher_cycle_operate() {}
  //void prefetcher_final_stats() {}
};

#endif

