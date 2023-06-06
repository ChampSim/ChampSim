/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CHANNEL_H
#define CHANNEL_H

#include <array>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <vector>

#include <string_view>

struct ooo_model_instr;

enum class access_type : unsigned {
  LOAD = 0,
  RFO,
  PREFETCH,
  WRITE,
  TRANSLATION,
  NUM_TYPES,
};

using namespace std::literals::string_view_literals;
inline constexpr std::array<std::string_view, static_cast<std::size_t>(access_type::NUM_TYPES)> access_type_names{"LOAD"sv, "RFO"sv, "PREFETCH"sv, "WRITE"sv,
                                                                                                                  "TRANSLATION"};

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

class channel
{
  struct request {
    bool forward_checked = false;
    bool is_translated = true;
    bool response_requested = true;

    uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};
    access_type type{access_type::LOAD};

    uint32_t pf_metadata = 0;
    uint32_t cpu = std::numeric_limits<uint32_t>::max();

    uint64_t address = 0;
    uint64_t v_address = 0;
    uint64_t data = 0;
    uint64_t instr_id = 0;
    uint64_t ip = 0;

    std::vector<std::reference_wrapper<ooo_model_instr>> instr_depend_on_me{};
  };

  struct response {
    uint64_t address;
    uint64_t v_address;
    uint64_t data;
    uint32_t pf_metadata = 0;
    std::vector<std::reference_wrapper<ooo_model_instr>> instr_depend_on_me{};

    response(uint64_t addr, uint64_t v_addr, uint64_t data_, uint32_t pf_meta, std::vector<std::reference_wrapper<ooo_model_instr>> deps)
        : address(addr), v_address(v_addr), data(data_), pf_metadata(pf_meta), instr_depend_on_me(deps)
    {
    }
    explicit response(request req) : response(req.address, req.v_address, req.data, req.pf_metadata, req.instr_depend_on_me) {}
  };

  template <typename R>
  bool do_add_queue(R& queue, std::size_t queue_size, const typename R::value_type& packet);

  std::size_t RQ_SIZE = std::numeric_limits<std::size_t>::max();
  std::size_t PQ_SIZE = std::numeric_limits<std::size_t>::max();
  std::size_t WQ_SIZE = std::numeric_limits<std::size_t>::max();
  unsigned OFFSET_BITS = 0;
  bool match_offset_bits = false;

public:
  using response_type = response;
  using request_type = request;
  using stats_type = cache_queue_stats;

  std::deque<request_type> RQ{}, PQ{}, WQ{};
  std::deque<response_type> returned{};

  stats_type sim_stats{}, roi_stats{};

  channel() = default;
  channel(std::size_t rq_size, std::size_t pq_size, std::size_t wq_size, unsigned offset_bits, bool match_offset);

  bool add_rq(const request_type& packet);
  bool add_wq(const request_type& packet);
  bool add_pq(const request_type& packet);

  std::size_t rq_occupancy() const;
  std::size_t wq_occupancy() const;
  std::size_t pq_occupancy() const;

  std::size_t rq_size() const;
  std::size_t wq_size() const;
  std::size_t pq_size() const;

  void check_collision();
};
} // namespace champsim

#endif
