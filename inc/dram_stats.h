#ifndef DRAM_STATS_H
#define DRAM_STATS_H

#include <cstdint>
#include <string>

struct dram_stats {
  std::string name{};
  uint64_t dbus_cycle_congested = 0, dbus_count_congested = 0;

  unsigned WQ_ROW_BUFFER_HIT = 0, WQ_ROW_BUFFER_MISS = 0, RQ_ROW_BUFFER_HIT = 0, RQ_ROW_BUFFER_MISS = 0, WQ_FULL = 0;
};

dram_stats operator-(dram_stats lhs, dram_stats rhs);

#endif
