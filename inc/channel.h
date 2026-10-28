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
#include <limits>
#include <string_view>
#include <vector>

#include "access_type.h"
#include "address.h"
#include "cache_stats.h"
#include "champsim.h"
#include "modules.h"
#include "packet.h"

namespace champsim
{

class channel : public champsim::modules::channel_module
{

  template <typename R>
  bool do_add_queue(R& queue, std::size_t queue_size, const typename R::value_type& packet);

  std::size_t RQ_SIZE = std::numeric_limits<std::size_t>::max();
  std::size_t PQ_SIZE = std::numeric_limits<std::size_t>::max();
  std::size_t WQ_SIZE = std::numeric_limits<std::size_t>::max();
  champsim::data::bits OFFSET_BITS{};
  bool match_offset_bits = false;

public:
  using response_type = response;
  using request_type = request;
  using stats_type = cache_queue_stats;

  std::deque<request_type> RQ{}, PQ{}, WQ{};
  std::deque<response_type> returned{};

  stats_type sim_stats{};

  channel();
  channel(champsim::modules::ModuleBuilder builder);

  bool add_rq(const request_type& packet) override;
  bool add_wq(const request_type& packet) override;
  bool add_pq(const request_type& packet) override;

  [[nodiscard]] std::size_t rq_occupancy() const override;
  [[nodiscard]] std::size_t wq_occupancy() const override;
  [[nodiscard]] std::size_t pq_occupancy() const override;

  [[nodiscard]] std::size_t rq_size() const override;
  [[nodiscard]] std::size_t wq_size() const override;
  [[nodiscard]] std::size_t pq_size() const override;

  std::deque<request_type>& get_rq() override { return RQ; }
  std::deque<request_type>& get_wq() override { return WQ; }
  std::deque<request_type>& get_pq() override { return PQ; }
  std::deque<response_type>& get_returned() override { return returned; }

  stats_type& get_sim_stats() override { return sim_stats; }
};
} // namespace champsim

#endif
