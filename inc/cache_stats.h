#ifndef CACHE_STATS_H
#define CACHE_STATS_H

#include <cstdint>
#include <string>
#include <utility>

#include "access_type.h"
#include "event_counter.h"

struct cache_stats {
  std::string name;
  // prefetch stats
  uint64_t pf_requested = 0;
  uint64_t pf_issued = 0;
  uint64_t pf_useful = 0;
  uint64_t pf_useless = 0;
  uint64_t pf_fill = 0;

  champsim::stats::event_counter<std::pair<access_type, std::size_t>> hits = {};
  champsim::stats::event_counter<std::pair<access_type, std::size_t>> misses = {};
  champsim::stats::event_counter<std::pair<access_type, std::size_t>> miss_merge = {};
  champsim::stats::event_counter<std::pair<access_type, std::size_t>> fill = {};

  long total_miss_latency_cycles{};
};

cache_stats operator-(cache_stats lhs, cache_stats rhs);

namespace champsim
{
struct cache_queue_stats {
  uint64_t RQ_ACCESS = 0;
  uint64_t RQ_FULL = 0;
  uint64_t RQ_TO_CACHE = 0;
  uint64_t PQ_ACCESS = 0;
  uint64_t PQ_FULL = 0;
  uint64_t PQ_TO_CACHE = 0;
  uint64_t WQ_ACCESS = 0;
  uint64_t WQ_FULL = 0;
  uint64_t WQ_TO_CACHE = 0;
};
} // namespace champsim

#endif
