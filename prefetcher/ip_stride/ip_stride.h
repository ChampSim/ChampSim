#ifndef IP_STRIDE_H
#define IP_STRIDE_H

#include <cstdint>
#include <optional>

#include "modules.h"
#include "msl/lru_table.h"

struct ip_stride : champsim::modules::prefetcher {
  struct tracker_entry {
    uint64_t ip = 0;           // the IP we're tracking
    uint64_t last_cl_addr = 0; // the last address accessed by this IP
    int64_t last_stride = 0;   // the stride between the last two addresses accessed by this IP

    auto index() const { return ip; }
    auto tag() const { return ip; }
  };

  struct lookahead_entry {
    uint64_t address = 0;
    int64_t stride = 0;
    int degree = 0; // degree remaining
  };

  constexpr static std::size_t TRACKER_SETS = 256;
  constexpr static std::size_t TRACKER_WAYS = 4;
  constexpr static int PREFETCH_DEGREE = 3;

  std::optional<lookahead_entry> active_lookahead;

  champsim::msl::lru_table<tracker_entry> table{TRACKER_SETS, TRACKER_WAYS};

public:
  using champsim::modules::prefetcher::prefetcher;

  uint32_t prefetcher_cache_operate(uint64_t addr, uint64_t ip, uint8_t cache_hit, bool useful_prefetch, access_type type, uint32_t metadata_in);
  uint32_t prefetcher_cache_fill(uint64_t addr, long set, long way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in);
  void prefetcher_cycle_operate();
};

#endif
