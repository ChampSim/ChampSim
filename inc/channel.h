#ifndef CHANNEL_H
#define CHANNEL_H

#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <vector>

#include "util.h"

struct ooo_model_instr;

enum access_type {
  LOAD = 0,
  RFO,
  PREFETCH,
  WRITE,
  TRANSLATION,
  NUM_TYPES,
};

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
};

struct channel {
  struct response;
  struct request
  {
    bool scheduled = false;
    bool forward_checked = false;
    bool translate_issued = false;
    bool prefetch_from_this = false;
    bool skip_fill = false;
    bool is_translated = true;

    uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()}, type = 0;

    uint32_t pf_metadata = 0;
    uint32_t cpu = std::numeric_limits<uint32_t>::max();

    uint64_t address = 0, v_address = 0, data = 0, instr_id = 0, ip = 0, event_cycle = std::numeric_limits<uint64_t>::max();

    std::vector<std::reference_wrapper<ooo_model_instr>> instr_depend_on_me{};
    std::vector<std::deque<response>*> to_return{};
  };

  struct response {
    uint64_t address;
    uint64_t v_address;
    uint64_t data;
    uint32_t pf_metadata = 0;
    std::vector<std::reference_wrapper<ooo_model_instr>> instr_depend_on_me{};

    response(uint64_t addr, uint64_t v_addr, uint64_t data_, uint32_t pf_meta, std::vector<std::reference_wrapper<ooo_model_instr>> deps) :
      address(addr), v_address(v_addr), data(data_), pf_metadata(pf_meta), instr_depend_on_me(deps) {}
    explicit response(request req) : address(req.address), v_address(req.v_address), data(req.data), pf_metadata(req.pf_metadata), instr_depend_on_me(req.instr_depend_on_me) {}
  };

  using response_type = response;
  using request_type = request;

  std::deque<request_type> RQ{}, PQ{}, WQ{};
  std::deque<response_type> returned{};
  const std::size_t RQ_SIZE = std::numeric_limits<std::size_t>::max();
  const std::size_t PQ_SIZE = std::numeric_limits<std::size_t>::max();
  const std::size_t WQ_SIZE = std::numeric_limits<std::size_t>::max();
  const unsigned OFFSET_BITS = 0;
  const bool match_offset_bits = false;

  using stats_type = cache_queue_stats;

  std::vector<stats_type> sim_stats, roi_stats;

  channel() = default;
  channel(std::size_t rq_size, std::size_t pq_size, std::size_t wq_size, unsigned offset_bits, bool match_offset);

  template <typename R>
    bool do_add_queue(R& queue, std::size_t queue_size, const request_type& packet);

  bool add_rq(const request_type& packet);
  bool add_wq(const request_type& packet);
  bool add_pq(const request_type& packet);

  void check_collision();
};
}

template <>
struct is_valid<champsim::channel::request_type> {
  bool operator()(const champsim::channel::request_type& test) { return test.address != 0; }
};

#endif
