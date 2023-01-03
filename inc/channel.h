#ifndef CHANNEL_H
#define CHANNEL_H

#include <cstdint>
#include <deque>
#include <limits>
#include <vector>

#include "memory_class.h"

namespace champsim
{
struct cache_queue_stats {
  uint64_t RQ_ACCESS = 0;
  uint64_t RQ_MERGED = 0;
  uint64_t RQ_FULL = 0;
  uint64_t RQ_TO_CACHE = 0;
  uint64_t PQ_ACCESS = 0;
  uint64_t PQ_MERGED = 0;
  uint64_t PQ_FULL = 0;
  uint64_t PQ_TO_CACHE = 0;
  uint64_t WQ_ACCESS = 0;
  uint64_t WQ_MERGED = 0;
  uint64_t WQ_FULL = 0;
  uint64_t WQ_TO_CACHE = 0;
  uint64_t WQ_FORWARD = 0;
  uint64_t PTWQ_ACCESS = 0;
  uint64_t PTWQ_MERGED = 0;
  uint64_t PTWQ_FULL = 0;
  uint64_t PTWQ_TO_CACHE = 0;
};

struct channel {
  std::deque<PACKET> RQ{}, PQ{}, WQ{}, PTWQ{};
  const std::size_t RQ_SIZE = std::numeric_limits<std::size_t>::max();
  const std::size_t PQ_SIZE = std::numeric_limits<std::size_t>::max();
  const std::size_t WQ_SIZE = std::numeric_limits<std::size_t>::max();
  const std::size_t PTWQ_SIZE = std::numeric_limits<std::size_t>::max();
  const unsigned OFFSET_BITS = 0;
  const bool match_offset_bits = false;

  using stats_type = cache_queue_stats;

  std::vector<stats_type> sim_stats, roi_stats;

  channel() = default;
  channel(std::size_t rq_size, std::size_t pq_size, std::size_t wq_size, std::size_t ptwq_size, unsigned offset_bits, bool match_offset);

  template <typename R>
  bool do_add_queue(R& queue, std::size_t queue_size, const PACKET& packet);

  bool add_rq(const PACKET& packet);
  bool add_wq(const PACKET& packet);
  bool add_pq(const PACKET& packet);
  bool add_ptwq(const PACKET& packet);

  void check_collision();
};
}

#endif
