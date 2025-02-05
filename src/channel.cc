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

#include "channel.h"

#include <cassert>
#include <fmt/core.h>

#include "cache.h"
#include "champsim.h"
#include "instruction.h"
#include "util/to_underlying.h" // for to_underlying

champsim::channel::channel(std::size_t rq_size, std::size_t pq_size, std::size_t wq_size, champsim::data::bits offset_bits, bool match_offset)
    : RQ_SIZE(rq_size), PQ_SIZE(pq_size), WQ_SIZE(wq_size), OFFSET_BITS(offset_bits), match_offset_bits(match_offset)
{
}

template <typename R>
bool champsim::channel::do_add_queue(R& queue, std::size_t queue_size, const typename R::value_type& packet)
{
  // check occupancy
  if (std::size(queue) >= queue_size) {
    return false; // cannot handle this request
  }

  // Insert the packet ahead of the translation misses
  auto fwd_pkt = packet;
  queue.push_back(fwd_pkt);

  return true;
}

bool champsim::channel::add_rq(const request_type& packet)
{
  if constexpr (champsim::debug_print) {
    fmt::print("[channel_rq] {} instr_id: {} address: {} v_address: {} type: {}\n", __func__, packet.instr_id, packet.address, packet.v_address,
               access_type_names.at(champsim::to_underlying(packet.type)));
  }

  sim_stats.RQ_ACCESS++;

  auto result = do_add_queue(RQ, RQ_SIZE, packet);

  if (result) {
    sim_stats.RQ_TO_CACHE++;
  } else {
    sim_stats.RQ_FULL++;
  }

  return result;
}

bool champsim::channel::add_wq(const request_type& packet)
{
  if constexpr (champsim::debug_print) {
    fmt::print("[channel_wq] {} instr_id: {} address: {} v_address: {} type: {}\n", __func__, packet.instr_id, packet.address, packet.v_address,
               access_type_names.at(champsim::to_underlying(packet.type)));
  }

  sim_stats.WQ_ACCESS++;

  auto result = do_add_queue(WQ, WQ_SIZE, packet);

  if (result) {
    sim_stats.WQ_TO_CACHE++;
  } else {
    sim_stats.WQ_FULL++;
  }

  return result;
}

bool champsim::channel::add_pq(const request_type& packet)
{
  if constexpr (champsim::debug_print) {
    fmt::print("[channel_pq] {} instr_id: {} address: {} v_address: {} type: {}\n", __func__, packet.instr_id, packet.address, packet.v_address,
               access_type_names.at(champsim::to_underlying(packet.type)));
  }

  sim_stats.PQ_ACCESS++;

  auto fwd_pkt = packet;
  auto result = do_add_queue(PQ, PQ_SIZE, fwd_pkt);
  if (result) {
    sim_stats.PQ_TO_CACHE++;
  } else {
    sim_stats.PQ_FULL++;
  }

  return result;
}

std::size_t champsim::channel::rq_occupancy() const { return std::size(RQ); }

std::size_t champsim::channel::wq_occupancy() const { return std::size(WQ); }

std::size_t champsim::channel::pq_occupancy() const { return std::size(PQ); }

std::size_t champsim::channel::rq_size() const { return RQ_SIZE; }

std::size_t champsim::channel::wq_size() const { return WQ_SIZE; }

std::size_t champsim::channel::pq_size() const { return PQ_SIZE; }
