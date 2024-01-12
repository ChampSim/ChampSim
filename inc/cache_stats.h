#ifndef CACHE_STATS_H
#define CACHE_STATS_H

#include "channel.h"
#include "event_counter.h"

#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

struct cache_stats {
  std::string name;
  // prefetch stats
  uint64_t pf_requested = 0;
  uint64_t pf_issued = 0;
  uint64_t pf_useful = 0;
  uint64_t pf_useless = 0;
  uint64_t pf_fill = 0;

  champsim::stats::event_counter<std::pair<access_type, std::remove_cv_t<decltype(NUM_CPUS)>>> hits = {};
  champsim::stats::event_counter<std::pair<access_type, std::remove_cv_t<decltype(NUM_CPUS)>>> misses = {};

  uint64_t total_miss_latency = 0;
};

cache_stats operator-(cache_stats lhs, cache_stats rhs);

#endif
