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

#include "cache.h"

#include <algorithm>
#include <iomanip>
#include <iterator>
#include <numeric>

#include <fmt/core.h>

#include "champsim.h"
#include "champsim_constants.h"
#include "instruction.h"
#include "util.h"

bool CACHE::handle_fill(const PACKET& fill_mshr)
{
  cpu = fill_mshr.cpu;

  // find victim
  auto [set_begin, set_end] = get_set_span(fill_mshr.address);
  auto way = std::find_if_not(set_begin, set_end, [](auto x) { return x.valid; });
  if (way == set_end)
    way = std::next(set_begin, impl_find_victim(fill_mshr.cpu, fill_mshr.instr_id, get_set_index(fill_mshr.address), &*set_begin, fill_mshr.ip,
                                                fill_mshr.address, fill_mshr.type));
  assert(set_begin <= way);
  assert(way <= set_end);
  const auto way_idx = static_cast<std::size_t>(std::distance(set_begin, way)); // cast protected by earlier assertion

  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} instr_id: {} address: {:x} full_addr: {:x} full_v_addr: {:x} set: {} way: {} type: {} cycle_enqueued: {} cycle: {}\n",
        NAME, __func__, fill_mshr.instr_id, fill_mshr.address >> OFFSET_BITS, fill_mshr.address, fill_mshr.v_address, get_set_index(fill_mshr.address),
        way_idx, +fill_mshr.type, fill_mshr.cycle_enqueued, current_cycle);
  }

  bool success = true;
  auto metadata_thru = fill_mshr.pf_metadata;
  auto pkt_address = (virtual_prefetch ? fill_mshr.v_address : fill_mshr.address) & ~champsim::bitmask(match_offset_bits ? 0 : OFFSET_BITS);
  if (way != set_end) {
    if (way->valid && way->dirty) {
      PACKET writeback_packet;

      writeback_packet.cpu = fill_mshr.cpu;
      writeback_packet.address = way->address;
      writeback_packet.data = way->data;
      writeback_packet.instr_id = fill_mshr.instr_id;
      writeback_packet.ip = 0;
      writeback_packet.type = WRITE;
      writeback_packet.pf_metadata = way->pf_metadata;

      success = lower_level->add_wq(writeback_packet);
    }

    if (success) {
      auto evicting_address = (ever_seen_data ? way->address : way->v_address) & ~champsim::bitmask(match_offset_bits ? 0 : OFFSET_BITS);

      if (way->prefetch)
        sim_stats.back().pf_useless++;

      if (fill_mshr.type == PREFETCH)
        sim_stats.back().pf_fill++;

      way->valid = true;
      way->prefetch = fill_mshr.prefetch_from_this;
      way->dirty = (fill_mshr.type == WRITE);
      way->address = fill_mshr.address;
      way->v_address = fill_mshr.v_address;
      way->data = fill_mshr.data;

      metadata_thru =
          impl_prefetcher_cache_fill(pkt_address, get_set_index(fill_mshr.address), way_idx, fill_mshr.type == PREFETCH, evicting_address, metadata_thru);
      impl_update_replacement_state(fill_mshr.cpu, get_set_index(fill_mshr.address), way_idx, fill_mshr.address, fill_mshr.ip, evicting_address, fill_mshr.type,
                                    false);

      way->pf_metadata = metadata_thru;
    }
  } else {
    // Bypass
    assert(fill_mshr.type != WRITE);

    metadata_thru = impl_prefetcher_cache_fill(pkt_address, get_set_index(fill_mshr.address), way_idx, fill_mshr.type == PREFETCH, 0, metadata_thru);
    impl_update_replacement_state(fill_mshr.cpu, get_set_index(fill_mshr.address), way_idx, fill_mshr.address, fill_mshr.ip, 0, fill_mshr.type, false);
  }

  if (success) {
    // COLLECT STATS
    sim_stats.back().total_miss_latency += current_cycle - (fill_mshr.cycle_enqueued + 1);

    auto copy{fill_mshr};
    copy.pf_metadata = metadata_thru;
    for (auto ret : copy.to_return)
      ret->return_data(copy);
  }

  return success;
}

bool CACHE::try_hit(const PACKET& handle_pkt)
{
  cpu = handle_pkt.cpu;

  // access cache
  auto [set_begin, set_end] = get_set_span(handle_pkt.address);
  auto way = std::find_if(set_begin, set_end, eq_addr<BLOCK>(handle_pkt.address, OFFSET_BITS));
  const auto hit = (way != set_end);

  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} instr_id: {} address: {:x} full_addr: {:x} full_v_addr: {:x} set: {} way: {} ({}) type: {} cycle: {}\n",
        NAME, __func__, handle_pkt.instr_id, handle_pkt.address >> OFFSET_BITS, handle_pkt.address, handle_pkt.v_address, get_set_index(handle_pkt.address),
        std::distance(set_begin, way), hit ? "HIT" : "MISS", +handle_pkt.type, current_cycle);
  }

  // update prefetcher on load instructions and prefetches from upper levels
  auto metadata_thru = handle_pkt.pf_metadata;
  if (should_activate_prefetcher(handle_pkt)) {
    uint64_t pf_base_addr = (virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~champsim::bitmask(match_offset_bits ? 0 : OFFSET_BITS);
    metadata_thru = impl_prefetcher_cache_operate(pf_base_addr, handle_pkt.ip, hit, handle_pkt.type, metadata_thru);
  }

  if (hit) {
    sim_stats.back().hits[handle_pkt.type][handle_pkt.cpu]++;

    // update replacement policy
    const auto way_idx = static_cast<std::size_t>(std::distance(set_begin, way)); // cast protected by earlier assertion
    impl_update_replacement_state(handle_pkt.cpu, get_set_index(handle_pkt.address), way_idx, way->address, handle_pkt.ip, 0, handle_pkt.type, true);

    auto copy{handle_pkt};
    copy.data = way->data;
    copy.pf_metadata = metadata_thru;
    for (auto ret : copy.to_return)
      ret->return_data(copy);

    way->dirty = (handle_pkt.type == WRITE);

    // update prefetch stats and reset prefetch bit
    if (way->prefetch && !handle_pkt.prefetch_from_this) {
      sim_stats.back().pf_useful++;
      way->prefetch = false;
    }
  } else {
    sim_stats.back().misses[handle_pkt.type][handle_pkt.cpu]++;
  }

  return hit;
}

bool CACHE::handle_miss(const PACKET& handle_pkt)
{
  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} instr_id: {} address: {:x} full_addr: {:x} full_v_addr: {:x} type: {} local_prefetch: {} cycle: {}\n",
        NAME, __func__, handle_pkt.instr_id, handle_pkt.address >> OFFSET_BITS, handle_pkt.address, handle_pkt.v_address,
        +handle_pkt.type, handle_pkt.prefetch_from_this, current_cycle);
  }

  cpu = handle_pkt.cpu;

  // check mshr
  auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(handle_pkt.address, OFFSET_BITS));
  bool mshr_full = (MSHR.size() == MSHR_SIZE);

  if (mshr_entry != MSHR.end()) // miss already inflight
  {
    auto instr_copy = std::move(mshr_entry->instr_depend_on_me);
    auto ret_copy = std::move(mshr_entry->to_return);

    std::set_union(std::begin(instr_copy), std::end(instr_copy), std::begin(handle_pkt.instr_depend_on_me), std::end(handle_pkt.instr_depend_on_me),
                   std::back_inserter(mshr_entry->instr_depend_on_me), ooo_model_instr::program_order);
    std::set_union(std::begin(ret_copy), std::end(ret_copy), std::begin(handle_pkt.to_return), std::end(handle_pkt.to_return),
                   std::back_inserter(mshr_entry->to_return));

    if (mshr_entry->type == PREFETCH && handle_pkt.type != PREFETCH) {
      // Mark the prefetch as useful
      if (mshr_entry->prefetch_from_this)
        sim_stats.back().pf_useful++;

      uint64_t prior_event_cycle = mshr_entry->event_cycle;
      auto to_return = std::move(mshr_entry->to_return);
      *mshr_entry = handle_pkt;

      // in case request is already returned, we should keep event_cycle
      mshr_entry->event_cycle = prior_event_cycle;
      mshr_entry->cycle_enqueued = current_cycle;
      mshr_entry->to_return = std::move(to_return);
    }
  } else {
    if (mshr_full)  // not enough MSHR resource
      return false; // TODO should we allow prefetches anyway if they will not be filled to this level?

    auto fwd_pkt = handle_pkt;

    if (fwd_pkt.type == WRITE)
      fwd_pkt.type = RFO;

    if (handle_pkt.fill_this_level)
      fwd_pkt.to_return = {this};
    else
      fwd_pkt.to_return.clear();

    fwd_pkt.fill_this_level = true; // We will always fill the lower level
    fwd_pkt.prefetch_from_this = false;

    bool success;
    if (prefetch_as_load || handle_pkt.type != PREFETCH)
      success = lower_level->add_rq(fwd_pkt);
    else
      success = lower_level->add_pq(fwd_pkt);

    if (!success)
      return false;

    // Allocate an MSHR
    if (!std::empty(fwd_pkt.to_return)) {
      mshr_entry = MSHR.insert(std::end(MSHR), handle_pkt);
      mshr_entry->pf_metadata = fwd_pkt.pf_metadata;
      mshr_entry->cycle_enqueued = current_cycle;
      mshr_entry->event_cycle = std::numeric_limits<uint64_t>::max();
    }
  }

  return true;
}

bool CACHE::handle_write(const PACKET& handle_pkt)
{
  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} instr_id: {} full_addr: {:x} full_v_addr: {:x} type: {} local_prefetch: {} cycle: {}\n",
        NAME, __func__, handle_pkt.instr_id, handle_pkt.address, handle_pkt.v_address, +handle_pkt.type, handle_pkt.prefetch_from_this, current_cycle);
  }

  inflight_writes.push_back(handle_pkt);
  inflight_writes.back().event_cycle = current_cycle + (warmup ? 0 : FILL_LATENCY);
  inflight_writes.back().cycle_enqueued = current_cycle;

  return true;
}

template <typename R, typename F>
long int operate_queue(R& queue, long int sz, F&& func)
{
  auto [begin, end] = champsim::get_span_p(std::cbegin(queue), std::cend(queue), sz, std::forward<F>(func));
  auto retval = std::distance(begin, end);
  queue.erase(begin, end);
  return retval;
}

void CACHE::operate()
{
  auto tag_bw = MAX_TAG;
  auto fill_bw = MAX_FILL;

  auto do_fill = [cycle = current_cycle, this](const auto& x) {
    return x.event_cycle <= cycle && this->handle_fill(x);
  };

  auto operate_readlike = [&, this](const auto& pkt) {
    return queues.is_ready(pkt) && (this->try_hit(pkt) || this->handle_miss(pkt));
  };

  auto operate_writelike = [&, this](const auto& pkt) {
    return queues.is_ready(pkt) && (this->try_hit(pkt) || this->handle_write(pkt));
  };

  for (auto q : {std::ref(MSHR), std::ref(inflight_writes)})
    fill_bw -= operate_queue(q.get(), fill_bw, do_fill);

  if (match_offset_bits) {
    // Treat writes (that is, stores) like reads
    for (auto q : {std::ref(queues.WQ), std::ref(queues.PTWQ), std::ref(queues.RQ), std::ref(queues.PQ)})
      tag_bw -= operate_queue(q.get(), tag_bw, operate_readlike);
  } else {
    // Treat writes (that is, writebacks) like fills
    tag_bw -= operate_queue(queues.WQ, tag_bw, operate_writelike);

    for (auto q : {std::ref(queues.PTWQ), std::ref(queues.RQ), std::ref(queues.PQ)})
      tag_bw -= operate_queue(q.get(), tag_bw, operate_readlike);
  }

  impl_prefetcher_cycle_operate();
}

uint64_t CACHE::get_set(uint64_t address) const { return get_set_index(address); }

std::size_t CACHE::get_set_index(uint64_t address) const { return (address >> OFFSET_BITS) & champsim::bitmask(champsim::lg2(NUM_SET)); }

template <typename It>
std::pair<It, It> get_span(It anchor, typename std::iterator_traits<It>::difference_type set_idx, typename std::iterator_traits<It>::difference_type num_way)
{
  auto begin = std::next(anchor, set_idx * num_way);
  return {std::move(begin), std::next(begin, num_way)};
}

auto CACHE::get_set_span(uint64_t address) -> std::pair<std::vector<BLOCK>::iterator, std::vector<BLOCK>::iterator>
{
  const auto set_idx = get_set_index(address);
  assert(set_idx < NUM_SET);
  return get_span(std::begin(block), static_cast<std::vector<BLOCK>::difference_type>(set_idx), NUM_WAY); // safe cast because of prior assert
}

auto CACHE::get_set_span(uint64_t address) const -> std::pair<std::vector<BLOCK>::const_iterator, std::vector<BLOCK>::const_iterator>
{
  const auto set_idx = get_set_index(address);
  assert(set_idx < NUM_SET);
  return get_span(std::cbegin(block), static_cast<std::vector<BLOCK>::difference_type>(set_idx), NUM_WAY); // safe cast because of prior assert
}

uint64_t CACHE::get_way(uint64_t address, uint64_t) const
{
  auto [begin, end] = get_set_span(address);
  return std::distance(begin, std::find_if(begin, end, eq_addr<BLOCK>(address, OFFSET_BITS)));
}

uint64_t CACHE::invalidate_entry(uint64_t inval_addr)
{
  auto [begin, end] = get_set_span(inval_addr);
  auto inv_way = std::find_if(begin, end, eq_addr<BLOCK>(inval_addr, OFFSET_BITS));

  if (inv_way != end)
    inv_way->valid = 0;

  return std::distance(begin, inv_way);
}

bool CACHE::add_rq(const PACKET& packet)
{
  if constexpr (champsim::debug_print) {
    fmt::print("[{}_RQ] {} instr_id: {} address: {:x} full_address: {:x} full_v_addr: {:x} type: {} occupancy: {} cycle: {}\n",
        NAME, __func__, packet.instr_id, packet.address >> OFFSET_BITS, packet.address, packet.v_address, +packet.type, std::size(queues.RQ), current_cycle);
  }

  return queues.add_rq(packet);
}

bool CACHE::add_wq(const PACKET& packet)
{
  if constexpr (champsim::debug_print) {
    fmt::print("[{}_WQ] {} instr_id: {} address: {:x} full_address: {:x} full_v_addr: {:x} type: {} occupancy: {} cycle: {}\n",
        NAME, __func__, packet.instr_id, packet.address >> OFFSET_BITS, packet.address, packet.v_address, +packet.type, std::size(queues.WQ), current_cycle);
  }

  return queues.add_wq(packet);
}

bool CACHE::add_ptwq(const PACKET& packet)
{
  if constexpr (champsim::debug_print) {
    fmt::print("[{}_PTWQ] {} instr_id: {} address: {:x} full_address: {:x} full_v_addr: {:x} type: {} occupancy: {} cycle: {}\n",
        NAME, __func__, packet.instr_id, packet.address >> OFFSET_BITS, packet.address, packet.v_address, +packet.type, std::size(queues.PTWQ), current_cycle);
  }

  return queues.add_ptwq(packet);
}

int CACHE::prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  sim_stats.back().pf_requested++;

  PACKET pf_packet;
  pf_packet.type = PREFETCH;
  pf_packet.prefetch_from_this = true;
  pf_packet.fill_this_level = fill_this_level;
  pf_packet.pf_metadata = prefetch_metadata;
  pf_packet.cpu = cpu;
  pf_packet.address = pf_addr;
  pf_packet.v_address = virtual_prefetch ? pf_addr : 0;

  auto success = this->add_pq(pf_packet);
  if (success)
    ++sim_stats.back().pf_issued;
  return success;
}

int CACHE::prefetch_line(uint64_t, uint64_t, uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  return prefetch_line(pf_addr, fill_this_level, prefetch_metadata);
}

bool CACHE::add_pq(const PACKET& packet)
{
  if constexpr (champsim::debug_print) {
    fmt::print("[{}_PQ] {} instr_id: {} address: {:x} full_address: {:x} full_v_addr: {:x} type: {} from_this: {} occupancy: {} cycle: {}\n",
        NAME, __func__, packet.instr_id, packet.address >> OFFSET_BITS, packet.address, packet.v_address, +packet.type, packet.prefetch_from_this, std::size(queues.PQ), current_cycle);
  }

  return queues.add_pq(packet);
}

void CACHE::return_data(const PACKET& packet)
{
  // check MSHR information
  auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(packet.address, OFFSET_BITS));
  auto first_unreturned = std::find_if(MSHR.begin(), MSHR.end(), [](auto x) { return x.event_cycle == std::numeric_limits<uint64_t>::max(); });

  // sanity check
  if (mshr_entry == MSHR.end()) {
    fmt::print(stderr, "[{}_MSHR] {} instr_id: {} cannot find a matching entry! address: {:x} v_address: {:x} event: {} current: {}\n", NAME, __func__, packet.instr_id, packet.address, packet.v_address, packet.event_cycle, current_cycle);
    assert(0);
  }

  // MSHR holds the most updated information about this request
  mshr_entry->data = packet.data;
  mshr_entry->pf_metadata = packet.pf_metadata;
  mshr_entry->event_cycle = current_cycle + (warmup ? 0 : FILL_LATENCY);

  if constexpr (champsim::debug_print) {
    fmt::print("[{}_MSHR] {} instr_id: {} address: {:x} data: {:x} event: {} current: {}\n", NAME, __func__, mshr_entry->instr_id, mshr_entry->address, mshr_entry->data, mshr_entry->event_cycle, current_cycle);
  }

  // Order this entry after previously-returned entries, but before non-returned
  // entries
  std::iter_swap(mshr_entry, first_unreturned);
}

std::size_t CACHE::get_occupancy(uint8_t queue_type, uint64_t)
{
  if (queue_type == 0)
    return std::size(MSHR);
  else if (queue_type == 1)
    return std::size(queues.RQ);
  else if (queue_type == 2)
    return std::size(queues.WQ);
  else if (queue_type == 3)
    return std::size(queues.PQ);

  return 0;
}

std::size_t CACHE::get_size(uint8_t queue_type, uint64_t)
{
  if (queue_type == 0)
    return MSHR_SIZE;
  else if (queue_type == 1)
    return queues.RQ_SIZE;
  else if (queue_type == 2)
    return queues.WQ_SIZE;
  else if (queue_type == 3)
    return queues.PQ_SIZE;
  else if (queue_type == 4)
    return queues.PTWQ_SIZE;

  return 0;
}

void CACHE::initialize()
{
  impl_prefetcher_initialize();
  impl_initialize_replacement();
}

void CACHE::begin_phase()
{
  roi_stats.emplace_back();
  sim_stats.emplace_back();

  roi_stats.back().name = NAME;
  sim_stats.back().name = NAME;
}

void CACHE::end_phase(unsigned finished_cpu)
{
  for (auto type : {LOAD, RFO, PREFETCH, WRITE, TRANSLATION}) {
    roi_stats.back().hits.at(type).at(finished_cpu) = sim_stats.back().hits.at(type).at(finished_cpu);
    roi_stats.back().misses.at(type).at(finished_cpu) = sim_stats.back().misses.at(type).at(finished_cpu);
  }

  roi_stats.back().pf_requested = sim_stats.back().pf_requested;
  roi_stats.back().pf_issued = sim_stats.back().pf_issued;
  roi_stats.back().pf_useful = sim_stats.back().pf_useful;
  roi_stats.back().pf_useless = sim_stats.back().pf_useless;
  roi_stats.back().pf_fill = sim_stats.back().pf_fill;

  roi_stats.back().total_miss_latency = sim_stats.back().total_miss_latency;
}

bool CACHE::should_activate_prefetcher(const PACKET& pkt) const { return ((1 << pkt.type) & pref_activate_mask) && !pkt.prefetch_from_this; }

void CACHE::print_deadlock()
{
  if (!std::empty(MSHR)) {
    std::size_t j = 0;
    for (PACKET entry : MSHR) {
      fmt::print("[{}_MSHR] entry: {} instr_id: {} address: {:x} v_addr: {:x} type: {} event_cycle: {}\n",
          NAME, j++, entry.instr_id, entry.address, entry.v_address, +entry.type, entry.event_cycle);
    }
  } else {
    fmt::print("{} MSHR empty\n", NAME);
  }

  if (!std::empty(queues.RQ)) {
    for (const auto& entry : queues.RQ) {
      fmt::print("[{}_RQ] instr_id: {} address: {:x} v_addr: {:x} type: {} event_cycle: {}\n",
          NAME, entry.instr_id, entry.address, entry.v_address, +entry.type, entry.event_cycle);
    }
  } else {
    fmt::print("{} RQ empty\n", NAME);
  }

  if (!std::empty(queues.WQ)) {
    for (const auto& entry : queues.WQ) {
      fmt::print("[{}_WQ] instr_id: {} address: {:x} v_addr: {:x} type: {} event_cycle: {}\n",
          NAME, entry.instr_id, entry.address, entry.v_address, +entry.type, entry.event_cycle);
    }
  } else {
    fmt::print("{} WQ empty\n", NAME);
  }

  if (!std::empty(queues.PQ)) {
    for (const auto& entry : queues.PQ) {
      fmt::print("[{}_PQ] instr_id: {} address: {:x} v_addr: {:x} type: {} event_cycle: {}\n",
          NAME, entry.instr_id, entry.address, entry.v_address, +entry.type, entry.event_cycle);
    }
  } else {
    fmt::print("{} PQ empty\n", NAME);
  }
}
