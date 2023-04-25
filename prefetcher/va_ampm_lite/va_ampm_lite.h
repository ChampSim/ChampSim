#ifndef PREFETCHER_VA_AMPM_LITE_H
#define PREFETCHER_VA_AMPM_LITE_H

#include <array>
#include <bitset>
#include <cstdint>

#include "modules.h"

struct va_ampm_lite : champsim::modules::prefetcher
{
  static constexpr std::size_t REGION_COUNT = 128;
  static constexpr int MAX_DISTANCE = 256;
  static constexpr int PREFETCH_DEGREE = 2;

  struct region_type {
    uint64_t vpn;
    std::bitset<PAGE_SIZE / BLOCK_SIZE> access_map{};
    std::bitset<PAGE_SIZE / BLOCK_SIZE> prefetch_map{};
    uint64_t lru;

    static uint64_t region_lru;

    region_type() : region_type(0) {}
    explicit region_type(uint64_t allocate_vpn) : vpn(allocate_vpn), lru(region_lru++) {}
  };

  using prefetcher::prefetcher;

  std::array<region_type, REGION_COUNT> regions;

  bool check_cl_access(uint64_t v_addr);
  bool check_cl_prefetch(uint64_t v_addr);

  static std::pair<uint64_t, uint64_t> page_and_offset(uint64_t addr);

  void prefetcher_initialize();
  uint32_t prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in);
  uint32_t prefetcher_cache_fill(uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in);

  //void prefetcher_cycle_operate() {}
  //void prefetcher_final_stats() {}
};

#endif

