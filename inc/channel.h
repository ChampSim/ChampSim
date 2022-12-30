#ifndef CHANNEL_H
#define CHANNEL_H

#include <cstdint>
#include <deque>
#include <vector>

#include "memory_class.h"
#include "operable.h"

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

struct NonTranslatingQueues : public operable {
  std::deque<PACKET> RQ, PQ, WQ, PTWQ;
  const std::size_t RQ_SIZE, PQ_SIZE, WQ_SIZE, PTWQ_SIZE;
  const uint64_t HIT_LATENCY;
  const unsigned OFFSET_BITS;
  const bool match_offset_bits;

  using stats_type = cache_queue_stats;

  std::vector<stats_type> sim_stats, roi_stats;

  NonTranslatingQueues(double freq_scale, std::size_t rq_size, std::size_t pq_size, std::size_t wq_size, std::size_t ptwq_size, uint64_t hit_latency,
                       unsigned offset_bits, bool match_offset)
      : champsim::operable(freq_scale), RQ_SIZE(rq_size), PQ_SIZE(pq_size), WQ_SIZE(wq_size), PTWQ_SIZE(ptwq_size), HIT_LATENCY(hit_latency),
        OFFSET_BITS(offset_bits), match_offset_bits(match_offset)
  {
  }
  void operate() override {};

  template <typename R>
  bool do_add_queue(R& queue, std::size_t queue_size, const PACKET& packet);

  bool add_rq(const PACKET& packet);
  bool add_wq(const PACKET& packet);
  bool add_pq(const PACKET& packet);
  bool add_ptwq(const PACKET& packet);

  void begin_phase() override;
  void end_phase(unsigned cpu) override;

  void check_collision();
};

class TranslatingQueues : public NonTranslatingQueues, public MemoryRequestProducer {
  std::deque<PACKET> returned{};

  public:
  void operate() override final;

  void issue_translation();
  void detect_misses();

  template <typename R>
  void do_detect_misses(R& queue);

  void finish_translation(const PACKET& packet);

  using NonTranslatingQueues::NonTranslatingQueues;
};
}

#endif
