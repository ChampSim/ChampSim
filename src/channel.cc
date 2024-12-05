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

template <typename Iter, typename F>
bool do_collision_for(Iter begin, Iter end, champsim::channel::request_type& packet, champsim::data::bits shamt, F&& func)
{
  // We make sure that both merge packet address have been translated. If
  // not this can happen: package with address virtual and physical X
  // (not translated) is inserted, package with physical address
  // (already translated) X.
  if (auto found =
          std::find_if(begin, end, [match = packet.address.slice_upper(shamt), shamt](const auto& x) { return x.address.slice_upper(shamt) == match; });
      found != end && packet.is_translated == found->is_translated) {
    func(packet, *found);
    return true;
  }

  return false;
}

template <typename Iter>
bool do_collision_for_merge(Iter begin, Iter end, champsim::channel::request_type& packet, champsim::data::bits shamt)
{
  return do_collision_for(begin, end, packet, shamt, [](champsim::channel::request_type& source, champsim::channel::request_type& destination) {
    destination.response_requested |= source.response_requested;
    auto instr_copy = std::move(destination.instr_depend_on_me);

    std::set_union(std::begin(instr_copy), std::end(instr_copy), std::begin(source.instr_depend_on_me), std::end(source.instr_depend_on_me),
                   std::back_inserter(destination.instr_depend_on_me));
  });
}

template <typename Iter>
bool do_collision_for_return(Iter begin, Iter end, champsim::channel::request_type& packet, champsim::data::bits shamt,
                             std::deque<champsim::channel::response_type>& returned)
{
  return do_collision_for(begin, end, packet, shamt, [&](champsim::channel::request_type& source, champsim::channel::request_type& destination) {
    if (source.response_requested) {
      returned.emplace_back(source.address, source.v_address, destination.data, destination.pf_metadata, source.instr_depend_on_me);
    }
  });
}

void champsim::channel::check_collision()
{
  auto write_shamt = match_offset_bits ? champsim::data::bits{} : OFFSET_BITS;
  auto read_shamt = OFFSET_BITS;

  // Check WQ for duplicates, merging if they are found
  for (auto wq_it = std::find_if(std::begin(WQ), std::end(WQ), std::not_fn(&request_type::forward_checked)); wq_it != std::end(WQ);) {
    if (do_collision_for_merge(std::begin(WQ), wq_it, *wq_it, write_shamt)) {
      sim_stats.WQ_MERGED++;
      wq_it = WQ.erase(wq_it);
    } else {
      wq_it->forward_checked = true;
      ++wq_it;
    }
  }

  // Check RQ for forwarding from WQ (return if found), then for duplicates (merge if found)
  for (auto rq_it = std::find_if(std::begin(RQ), std::end(RQ), std::not_fn(&request_type::forward_checked)); rq_it != std::end(RQ);) {
    if (do_collision_for_return(std::begin(WQ), std::end(WQ), *rq_it, write_shamt, returned)) {
      sim_stats.WQ_FORWARD++;
      rq_it = RQ.erase(rq_it);
    } else if (do_collision_for_merge(std::begin(RQ), rq_it, *rq_it, read_shamt)) {
      sim_stats.RQ_MERGED++;
      rq_it = RQ.erase(rq_it);
    } else {
      rq_it->forward_checked = true;
      ++rq_it;
    }
  }

  // Check PQ for forwarding from WQ (return if found), then for duplicates (merge if found)
  for (auto pq_it = std::find_if(std::begin(PQ), std::end(PQ), std::not_fn(&request_type::forward_checked)); pq_it != std::end(PQ);) {
    if (do_collision_for_return(std::begin(WQ), std::end(WQ), *pq_it, write_shamt, returned)) {
      sim_stats.WQ_FORWARD++;
      pq_it = PQ.erase(pq_it);
    } else if (do_collision_for_merge(std::begin(PQ), pq_it, *pq_it, read_shamt)) {
      sim_stats.PQ_MERGED++;
      pq_it = PQ.erase(pq_it);
    } else {
      pq_it->forward_checked = true;
      ++pq_it;
    }
  }
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
  fwd_pkt.forward_checked = false;
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
