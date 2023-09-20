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
#include <cassert>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <fmt/core.h>
#include <fmt/ranges.h>

#include "champsim.h"
#include "champsim_constants.h"
#include "deadlock.h"
#include "instruction.h"
#include "util/algorithm.h"
#include "util/span.h"
#include <fmt/core.h>

CACHE::tag_lookup_type::tag_lookup_type(request_type req, bool local_pref, bool skip)
    : address(req.address), v_address(req.v_address), data(req.data), ip(req.ip), instr_id(req.instr_id), pf_metadata(req.pf_metadata), cpu(req.cpu),
      type(req.type), prefetch_from_this(local_pref), skip_fill(skip), is_translated(req.is_translated), instr_depend_on_me(req.instr_depend_on_me)
{
}

CACHE::mshr_type::mshr_type(tag_lookup_type req, uint64_t cycle)
    : address(req.address), v_address(req.v_address), data(req.data), ip(req.ip), instr_id(req.instr_id), pf_metadata(req.pf_metadata), cpu(req.cpu),
      type(req.type), prefetch_from_this(req.prefetch_from_this), cycle_enqueued(cycle), instr_depend_on_me(req.instr_depend_on_me), to_return(req.to_return)
{
}

CACHE::mshr_type CACHE::mshr_type::merge(mshr_type predecessor, mshr_type successor)
{
  std::vector<std::reference_wrapper<ooo_model_instr>> merged_instr{};
  std::vector<std::deque<response_type>*> merged_return{};

  std::set_union(std::begin(predecessor.instr_depend_on_me), std::end(predecessor.instr_depend_on_me), std::begin(successor.instr_depend_on_me),
                 std::end(successor.instr_depend_on_me), std::back_inserter(merged_instr), ooo_model_instr::program_order);
  std::set_union(std::begin(predecessor.to_return), std::end(predecessor.to_return), std::begin(successor.to_return), std::end(successor.to_return),
                 std::back_inserter(merged_return));

  mshr_type retval{(successor.type == access_type::PREFETCH) ? predecessor : successor};
  retval.instr_depend_on_me = merged_instr;
  retval.to_return = merged_return;
  retval.data = predecessor.data;

  if (predecessor.event_cycle < std::numeric_limits<uint64_t>::max()) {
    retval.event_cycle = predecessor.event_cycle;
  }

  if constexpr (champsim::debug_print) {
    if (successor.type == access_type::PREFETCH) {
      fmt::print("[MSHR] {} address {:#x} type: {} into address {:#x} type: {} event: {}\n", __func__, successor.address, access_type_names.at(champsim::to_underlying(successor.type)), predecessor.address, access_type_names.at(champsim::to_underlying(successor.type)), retval.event_cycle);
    } else {
      fmt::print("[MSHR] {} address {:#x} type: {} into address {:#x} type: {} event: {}\n", __func__, predecessor.address, access_type_names.at(champsim::to_underlying(predecessor.type)), successor.address, access_type_names.at(champsim::to_underlying(successor.type)), retval.event_cycle);
    }
  }

  return retval;
}

CACHE::BLOCK::BLOCK(mshr_type mshr)
    : valid(true), prefetch(mshr.prefetch_from_this), dirty(mshr.type == access_type::WRITE), address(mshr.address), v_address(mshr.v_address), data(mshr.data)
{
}

bool CACHE::handle_fill(const mshr_type& fill_mshr)
{
  cpu = fill_mshr.cpu;

  // find victim
  auto [set_begin, set_end] = get_set_span(fill_mshr.address);
  auto way = std::find_if_not(set_begin, set_end, [](auto x) { return x.valid; });
  if (way == set_end)
    way = std::next(set_begin, impl_find_victim(fill_mshr.cpu, fill_mshr.instr_id, get_set_index(fill_mshr.address), &*set_begin, fill_mshr.ip,
                                                fill_mshr.address, champsim::to_underlying(fill_mshr.type)));
  assert(set_begin <= way);
  assert(way <= set_end);
  const auto way_idx = static_cast<std::size_t>(std::distance(set_begin, way)); // cast protected by earlier assertion

  if constexpr (champsim::debug_print) {
    fmt::print(
        "[{}] {} instr_id: {} address: {:#x} v_address: {:#x} set: {} way: {} type: {} prefetch_metadata: {} cycle_enqueued: {} cycle: {}\n",
        NAME, __func__, fill_mshr.instr_id, fill_mshr.address, fill_mshr.v_address, get_set_index(fill_mshr.address), way_idx,
        access_type_names.at(champsim::to_underlying(fill_mshr.type)), fill_mshr.pf_metadata, fill_mshr.cycle_enqueued, current_cycle);
  }

  bool success = true;
  auto metadata_thru = fill_mshr.pf_metadata;
  auto pkt_address = (virtual_prefetch ? fill_mshr.v_address : fill_mshr.address) & ~champsim::bitmask(match_offset_bits ? 0 : OFFSET_BITS);
  if (way != set_end) {
    if (way->valid && way->dirty) {
      request_type writeback_packet;

      writeback_packet.cpu = fill_mshr.cpu;
      writeback_packet.address = way->address;
      writeback_packet.data = way->data;
      writeback_packet.instr_id = fill_mshr.instr_id;
      writeback_packet.ip = 0;
      writeback_packet.type = access_type::WRITE;
      writeback_packet.pf_metadata = way->pf_metadata;
      writeback_packet.response_requested = false;

      if constexpr (champsim::debug_print) {
        fmt::print("[{}] {} evict address: {:#x} v_address: {:#x} prefetch_metadata: {}\n", NAME,
            __func__, writeback_packet.address, writeback_packet.v_address, fill_mshr.pf_metadata);
      }

      success = lower_level->add_wq(writeback_packet);
    }

    if (success) {
      auto evicting_address = (ever_seen_data ? way->address : way->v_address) & ~champsim::bitmask(match_offset_bits ? 0 : OFFSET_BITS);

      if (way->prefetch)
        ++sim_stats.pf_useless;

      if (fill_mshr.type == access_type::PREFETCH)
        ++sim_stats.pf_fill;

      *way = BLOCK{fill_mshr};

      metadata_thru = impl_prefetcher_cache_fill(pkt_address, get_set_index(fill_mshr.address), way_idx, fill_mshr.type == access_type::PREFETCH,
                                                 evicting_address, metadata_thru);
      impl_update_replacement_state(fill_mshr.cpu, get_set_index(fill_mshr.address), way_idx, fill_mshr.address, fill_mshr.ip, evicting_address,
                                    champsim::to_underlying(fill_mshr.type), false);

      way->pf_metadata = metadata_thru;
    }
  } else {
    // Bypass
    assert(fill_mshr.type != access_type::WRITE);

    metadata_thru =
        impl_prefetcher_cache_fill(pkt_address, get_set_index(fill_mshr.address), way_idx, fill_mshr.type == access_type::PREFETCH, 0, metadata_thru);
    impl_update_replacement_state(fill_mshr.cpu, get_set_index(fill_mshr.address), way_idx, fill_mshr.address, fill_mshr.ip, 0,
                                  champsim::to_underlying(fill_mshr.type), false);
  }

  if (success) {
    // COLLECT STATS
    sim_stats.total_miss_latency += current_cycle - (fill_mshr.cycle_enqueued + 1);

    response_type response{fill_mshr.address, fill_mshr.v_address, fill_mshr.data, metadata_thru, fill_mshr.instr_depend_on_me};
    for (auto ret : fill_mshr.to_return)
      ret->push_back(response);
  }

  return success;
}

bool CACHE::try_hit(const tag_lookup_type& handle_pkt)
{
  cpu = handle_pkt.cpu;

  // access cache
  auto [set_begin, set_end] = get_set_span(handle_pkt.address);
  auto way = std::find_if(set_begin, set_end,
                          [match = handle_pkt.address >> OFFSET_BITS, shamt = OFFSET_BITS](const auto& entry) { return (entry.address >> shamt) == match; });
  const auto hit = (way != set_end);
  const auto useful_prefetch = (hit && way->prefetch && !handle_pkt.prefetch_from_this);

  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} instr_id: {} address: {:#x} v_address: {:#x} data: {:#x} set: {} way: {} ({}) type: {} cycle: {}\n", NAME, __func__, handle_pkt.instr_id,
               handle_pkt.address, handle_pkt.v_address, handle_pkt.data, get_set_index(handle_pkt.address), std::distance(set_begin, way), hit ? "HIT" : "MISS",
               access_type_names.at(champsim::to_underlying(handle_pkt.type)), current_cycle);
  }

  // update prefetcher on load instructions and prefetches from upper levels
  auto metadata_thru = handle_pkt.pf_metadata;
  if (should_activate_prefetcher(handle_pkt)) {
    uint64_t pf_base_addr = (virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~champsim::bitmask(match_offset_bits ? 0 : OFFSET_BITS);
    metadata_thru = impl_prefetcher_cache_operate(pf_base_addr, handle_pkt.ip, hit, useful_prefetch, champsim::to_underlying(handle_pkt.type), metadata_thru);
  }

  if (hit) {
    ++sim_stats.hits[champsim::to_underlying(handle_pkt.type)][handle_pkt.cpu];

    // update replacement policy
    const auto way_idx = static_cast<std::size_t>(std::distance(set_begin, way)); // cast protected by earlier assertion
    impl_update_replacement_state(handle_pkt.cpu, get_set_index(handle_pkt.address), way_idx, way->address, handle_pkt.ip, 0,
                                  champsim::to_underlying(handle_pkt.type), true);

    response_type response{handle_pkt.address, handle_pkt.v_address, way->data, metadata_thru, handle_pkt.instr_depend_on_me};
    for (auto ret : handle_pkt.to_return)
      ret->push_back(response);

    way->dirty |= (handle_pkt.type == access_type::WRITE);

    // update prefetch stats and reset prefetch bit
    if (useful_prefetch) {
      ++sim_stats.pf_useful;
      way->prefetch = false;
    }
  }

  return hit;
}

bool CACHE::handle_miss(const tag_lookup_type& handle_pkt)
{
  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} instr_id: {} address: {:#x} v_address: {:#x} type: {} local_prefetch: {} cycle: {}\n", NAME, __func__,
               handle_pkt.instr_id, handle_pkt.address, handle_pkt.v_address,
               access_type_names.at(champsim::to_underlying(handle_pkt.type)), handle_pkt.prefetch_from_this, current_cycle);
  }

  mshr_type to_allocate{handle_pkt, current_cycle};

  cpu = handle_pkt.cpu;

  // check mshr
  auto mshr_entry = std::find_if(std::begin(MSHR), std::end(MSHR), [match = handle_pkt.address >> OFFSET_BITS, shamt = OFFSET_BITS](const auto& entry) {
    return (entry.address >> shamt) == match;
  });
  bool mshr_full = (MSHR.size() == MSHR_SIZE);

  if (mshr_entry != MSHR.end()) // miss already inflight
  {
    if (mshr_entry->type == access_type::PREFETCH && handle_pkt.type != access_type::PREFETCH) {
      // Mark the prefetch as useful
      if (mshr_entry->prefetch_from_this)
        ++sim_stats.pf_useful;
    }

    *mshr_entry = mshr_type::merge(*mshr_entry, to_allocate);
  } else {
    if (mshr_full) { // not enough MSHR resource
      if constexpr (champsim::debug_print) {
        fmt::print("[{}] {} MSHR full\n", NAME, __func__);
      }

      return false;  // TODO should we allow prefetches anyway if they will not be filled to this level?
    }

    request_type fwd_pkt;

    fwd_pkt.asid[0] = handle_pkt.asid[0];
    fwd_pkt.asid[1] = handle_pkt.asid[1];
    fwd_pkt.type = (handle_pkt.type == access_type::WRITE) ? access_type::RFO : handle_pkt.type;
    fwd_pkt.pf_metadata = handle_pkt.pf_metadata;
    fwd_pkt.cpu = handle_pkt.cpu;

    fwd_pkt.address = handle_pkt.address;
    fwd_pkt.v_address = handle_pkt.v_address;
    fwd_pkt.data = handle_pkt.data;
    fwd_pkt.instr_id = handle_pkt.instr_id;
    fwd_pkt.ip = handle_pkt.ip;

    fwd_pkt.instr_depend_on_me = handle_pkt.instr_depend_on_me;
    fwd_pkt.response_requested = (!handle_pkt.prefetch_from_this || !handle_pkt.skip_fill);

    bool success;
    if (prefetch_as_load || handle_pkt.type != access_type::PREFETCH)
      success = lower_level->add_rq(fwd_pkt);
    else
      success = lower_level->add_pq(fwd_pkt);

    if (!success) {
      if constexpr (champsim::debug_print) {
        fmt::print("[{}] {} could not send to lower\n", NAME, __func__);
      }

      return false;
    }

    // Allocate an MSHR
    if (fwd_pkt.response_requested) {
      MSHR.push_back(to_allocate);
      MSHR.back().pf_metadata = fwd_pkt.pf_metadata;
    }
  }

  ++sim_stats.misses[champsim::to_underlying(handle_pkt.type)][handle_pkt.cpu];

  return true;
}

bool CACHE::handle_write(const tag_lookup_type& handle_pkt)
{
  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} instr_id: {} address: {:#x} v_address: {:#x} type: {} local_prefetch: {} cycle: {}\n", NAME, __func__, handle_pkt.instr_id,
               handle_pkt.address, handle_pkt.v_address, access_type_names.at(champsim::to_underlying(handle_pkt.type)), handle_pkt.prefetch_from_this,
               current_cycle);
  }

  inflight_writes.emplace_back(handle_pkt, current_cycle);
  inflight_writes.back().event_cycle = current_cycle + (warmup ? 0 : FILL_LATENCY);
    
  ++sim_stats.misses[champsim::to_underlying(handle_pkt.type)][handle_pkt.cpu];

  return true;
}

template <bool UpdateRequest>
auto CACHE::initiate_tag_check(champsim::channel* ul)
{
  return [cycle = current_cycle + (warmup ? 0 : HIT_LATENCY), ul](const auto& entry) {
    CACHE::tag_lookup_type retval{entry};
    retval.event_cycle = cycle;

    if constexpr (UpdateRequest) {
      if (entry.response_requested)
        retval.to_return = {&ul->returned};
    }

    if constexpr (champsim::debug_print) {
      fmt::print("[TAG] initiate_tag_check instr_id: {} address: {:#x} v_address: {:#x} type: {} response_requested: {} event: {}\n", retval.instr_id, retval.address,
                 retval.v_address, access_type_names.at(champsim::to_underlying(retval.type)), !std::empty(retval.to_return), retval.event_cycle);
    }

    return retval;
  };
}

long CACHE::operate()
{
  long progress{0};

  for (auto ul : upper_levels)
    ul->check_collision();

  // Finish returns
  std::for_each(std::cbegin(lower_level->returned), std::cend(lower_level->returned), [this](const auto& pkt) { this->finish_packet(pkt); });
  progress += std::distance(std::cbegin(lower_level->returned), std::cend(lower_level->returned));
  lower_level->returned.clear();

  // Finish translations
  if (lower_translate != nullptr) {
    std::for_each(std::cbegin(lower_translate->returned), std::cend(lower_translate->returned), [this](const auto& pkt) { this->finish_translation(pkt); });
    progress += std::distance(std::cbegin(lower_translate->returned), std::cend(lower_translate->returned));
    lower_translate->returned.clear();
  }

  // Perform fills
  auto fill_bw = MAX_FILL;
  for (auto q : {std::ref(MSHR), std::ref(inflight_writes)}) {
    auto [fill_begin, fill_end] =
        champsim::get_span_p(std::cbegin(q.get()), std::cend(q.get()), fill_bw, [cycle = current_cycle](const auto& x) { return x.event_cycle <= cycle; });
    auto complete_end = std::find_if_not(fill_begin, fill_end, [this](const auto& x) { return this->handle_fill(x); });
    fill_bw -= std::distance(fill_begin, complete_end);
    q.get().erase(fill_begin, complete_end);
  }
  progress += MAX_FILL - fill_bw;

  // Initiate tag checks
  auto tag_bw = std::max(0ll, std::min<long long>(static_cast<long long>(MAX_TAG), MAX_TAG * HIT_LATENCY - std::size(inflight_tag_check)));
  auto can_translate = [avail = (std::size(translation_stash) < static_cast<std::size_t>(MSHR_SIZE))](const auto& entry) {
    return avail || entry.is_translated;
  };
  auto stash_bandwidth_consumed = champsim::transform_while_n(
      translation_stash, std::back_inserter(inflight_tag_check), tag_bw, [](const auto& entry) { return entry.is_translated; }, initiate_tag_check<false>());
  tag_bw -= stash_bandwidth_consumed;
  progress += stash_bandwidth_consumed;
  std::vector<long long> channels_bandwidth_consumed{};
  for (auto* ul : upper_levels) {
    for (auto q : {std::ref(ul->WQ), std::ref(ul->RQ), std::ref(ul->PQ)}) {
      auto bandwidth_consumed = champsim::transform_while_n(q.get(), std::back_inserter(inflight_tag_check), tag_bw, can_translate, initiate_tag_check<true>(ul));
      channels_bandwidth_consumed.push_back(bandwidth_consumed);
      tag_bw -= bandwidth_consumed;
      progress += bandwidth_consumed;
    }
  }
  auto pq_bandwidth_consumed = champsim::transform_while_n(internal_PQ, std::back_inserter(inflight_tag_check), tag_bw, can_translate, initiate_tag_check<false>());
  tag_bw -= pq_bandwidth_consumed;
  progress += pq_bandwidth_consumed;

  // Issue translations
  issue_translation();

  // Find entries that would be ready except that they have not finished translation, move them to the stash
  auto [last_not_missed, stash_end] =
      champsim::extract_if(std::begin(inflight_tag_check), std::end(inflight_tag_check), std::back_inserter(translation_stash),
                           [cycle = current_cycle](const auto& x) { return x.event_cycle < cycle && !x.is_translated; });
  progress += std::distance(last_not_missed, std::end(inflight_tag_check));
  inflight_tag_check.erase(last_not_missed, std::end(inflight_tag_check));

  // Perform tag checks
  auto do_tag_check = [this](const auto& pkt) {
    if (this->try_hit(pkt))
      return true;
    if (pkt.type == access_type::WRITE && !this->match_offset_bits)
      return this->handle_write(pkt); // Treat writes (that is, writebacks) like fills
    else
      return this->handle_miss(pkt); // Treat writes (that is, stores) like reads
  };
  auto [tag_check_ready_begin, tag_check_ready_end] =
      champsim::get_span_p(std::begin(inflight_tag_check), std::end(inflight_tag_check), MAX_TAG,
                           [cycle = current_cycle](const auto& pkt) { return pkt.event_cycle <= cycle && pkt.is_translated; });
  auto finish_tag_check_end = std::find_if_not(tag_check_ready_begin, tag_check_ready_end, do_tag_check);
  auto tag_bw_consumed = std::distance(tag_check_ready_begin, finish_tag_check_end);
  progress += std::distance(tag_check_ready_begin, finish_tag_check_end);
  inflight_tag_check.erase(tag_check_ready_begin, finish_tag_check_end);

  impl_prefetcher_cycle_operate();

  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} cycle completed: {} tags checked: {} remaining: {} stash consumed: {} remaining: {} channel consumed: {} pq consumed {} unused consume bw {}\n", NAME, __func__, current_cycle,
        tag_bw_consumed, std::size(inflight_tag_check),
        stash_bandwidth_consumed, std::size(translation_stash),
        channels_bandwidth_consumed, pq_bandwidth_consumed, tag_bw);
  }

  return progress;
}

// LCOV_EXCL_START exclude deprecated function
uint64_t CACHE::get_set(uint64_t address) const { return get_set_index(address); }
// LCOV_EXCL_STOP

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

// LCOV_EXCL_START exclude deprecated function
uint64_t CACHE::get_way(uint64_t address, uint64_t) const
{
  auto [begin, end] = get_set_span(address);
  return std::distance(
      begin, std::find_if(begin, end, [match = address >> OFFSET_BITS, shamt = OFFSET_BITS](const auto& entry) { return (entry.address >> shamt) == match; }));
}
// LCOV_EXCL_STOP

uint64_t CACHE::invalidate_entry(uint64_t inval_addr)
{
  auto [begin, end] = get_set_span(inval_addr);
  auto inv_way =
      std::find_if(begin, end, [match = inval_addr >> OFFSET_BITS, shamt = OFFSET_BITS](const auto& entry) { return (entry.address >> shamt) == match; });

  if (inv_way != end)
    inv_way->valid = 0;

  return std::distance(begin, inv_way);
}

int CACHE::prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  ++sim_stats.pf_requested;

  if (std::size(internal_PQ) >= PQ_SIZE)
    return false;

  request_type pf_packet;
  pf_packet.type = access_type::PREFETCH;
  pf_packet.pf_metadata = prefetch_metadata;
  pf_packet.cpu = cpu;
  pf_packet.address = pf_addr;
  pf_packet.v_address = virtual_prefetch ? pf_addr : 0;
  pf_packet.is_translated = !virtual_prefetch;

  internal_PQ.emplace_back(pf_packet, true, !fill_this_level);
  ++sim_stats.pf_issued;

  return true;
}

// LCOV_EXCL_START exclude deprecated function
int CACHE::prefetch_line(uint64_t, uint64_t, uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  return prefetch_line(pf_addr, fill_this_level, prefetch_metadata);
}
// LCOV_EXCL_STOP

void CACHE::finish_packet(const response_type& packet)
{
  // check MSHR information
  auto mshr_entry = std::find_if(std::begin(MSHR), std::end(MSHR),
                                 [match = packet.address >> OFFSET_BITS, shamt = OFFSET_BITS](const auto& entry) { return (entry.address >> shamt) == match; });
  auto first_unreturned = std::find_if(MSHR.begin(), MSHR.end(), [](auto x) { return x.event_cycle == std::numeric_limits<uint64_t>::max(); });

  // sanity check
  if (mshr_entry == MSHR.end()) {
    fmt::print(stderr, "[{}_MSHR] {} cannot find a matching entry! address: {:#x} v_address: {:#x}\n", NAME, __func__, packet.address, packet.v_address);
    assert(0);
  }

  // MSHR holds the most updated information about this request
  mshr_entry->data = packet.data;
  mshr_entry->pf_metadata = packet.pf_metadata;
  mshr_entry->event_cycle = current_cycle + (warmup ? 0 : FILL_LATENCY);

  if constexpr (champsim::debug_print) {
    fmt::print("[{}_MSHR] {} instr_id: {} address: {:#x} data: {:#x} type: {} to_finish: {} event: {} current: {}\n", NAME, __func__, mshr_entry->instr_id,
               mshr_entry->address, mshr_entry->data, access_type_names.at(champsim::to_underlying(mshr_entry->type)), std::size(lower_level->returned),
               mshr_entry->event_cycle, current_cycle);
  }

  // Order this entry after previously-returned entries, but before non-returned
  // entries
  std::iter_swap(mshr_entry, first_unreturned);
}

void CACHE::finish_translation(const response_type& packet)
{
  auto matches_vpage = [page_num = packet.v_address >> LOG2_PAGE_SIZE](const auto& entry) {
    return (entry.v_address >> LOG2_PAGE_SIZE) == page_num;
  };
  auto mark_translated = [p_page = packet.data, this](auto& entry) {
    entry.address = champsim::splice_bits(p_page, entry.v_address, LOG2_PAGE_SIZE); // translated address
    entry.is_translated = true;                                                     // This entry is now translated

    if constexpr (champsim::debug_print) {
      fmt::print("[{}_TRANSLATE] finish_translation paddr: {:#x} vaddr: {:#x} cycle: {}\n", this->NAME, entry.address, entry.v_address, this->current_cycle);
    }
  };

  // Restart stashed translations
  auto finish_begin = std::find_if_not(std::begin(translation_stash), std::end(translation_stash), [](const auto& x) { return x.is_translated; });
  auto finish_end = std::stable_partition(finish_begin, std::end(translation_stash), matches_vpage);
  std::for_each(finish_begin, finish_end, mark_translated);

  // Find all packets that match the page of the returned packet
  for (auto& entry : inflight_tag_check) {
    if ((entry.v_address >> LOG2_PAGE_SIZE) == (packet.v_address >> LOG2_PAGE_SIZE)) {
      mark_translated(entry);
    }
  }
}

void CACHE::issue_translation()
{
  auto issue = [this](auto& q_entry) {
    if (!q_entry.translate_issued && !q_entry.is_translated) {
      request_type fwd_pkt;
      fwd_pkt.asid[0] = q_entry.asid[0];
      fwd_pkt.asid[1] = q_entry.asid[1];
      fwd_pkt.type = access_type::LOAD;
      fwd_pkt.cpu = q_entry.cpu;

      fwd_pkt.address = q_entry.address;
      fwd_pkt.v_address = q_entry.v_address;
      fwd_pkt.data = q_entry.data;
      fwd_pkt.instr_id = q_entry.instr_id;
      fwd_pkt.ip = q_entry.ip;

      fwd_pkt.instr_depend_on_me = q_entry.instr_depend_on_me;
      fwd_pkt.is_translated = true;

      q_entry.translate_issued = this->lower_translate->add_rq(fwd_pkt);
      if constexpr (champsim::debug_print) {
        if (q_entry.translate_issued) {
          fmt::print("[TRANSLATE] do_issue_translation instr_id: {} paddr: {:#x} vaddr: {:#x} cycle: {}\n", q_entry.instr_id, q_entry.address, q_entry.v_address,
                     access_type_names.at(champsim::to_underlying(q_entry.type)));
        }
      }
    }
  };

  std::for_each(std::begin(inflight_tag_check), std::end(inflight_tag_check), issue);
  std::for_each(std::begin(translation_stash), std::end(translation_stash), issue);
}

std::size_t CACHE::get_mshr_occupancy() const { return std::size(MSHR); }

std::vector<std::size_t> CACHE::get_rq_occupancy() const
{
  std::vector<std::size_t> retval;
  std::transform(std::begin(upper_levels), std::end(upper_levels), std::back_inserter(retval), [](auto ulptr) { return ulptr->rq_occupancy(); });
  return retval;
}

std::vector<std::size_t> CACHE::get_wq_occupancy() const
{
  std::vector<std::size_t> retval;
  std::transform(std::begin(upper_levels), std::end(upper_levels), std::back_inserter(retval), [](auto ulptr) { return ulptr->wq_occupancy(); });
  return retval;
}

std::vector<std::size_t> CACHE::get_pq_occupancy() const
{
  std::vector<std::size_t> retval;
  std::transform(std::begin(upper_levels), std::end(upper_levels), std::back_inserter(retval), [](auto ulptr) { return ulptr->pq_occupancy(); });
  retval.push_back(std::size(internal_PQ));
  return retval;
}

// LCOV_EXCL_START exclude deprecated function
std::size_t CACHE::get_occupancy(uint8_t queue_type, uint64_t)
{
  if (queue_type == 0)
    return get_mshr_occupancy();
  return 0;
}
// LCOV_EXCL_STOP

std::size_t CACHE::get_mshr_size() const { return MSHR_SIZE; }

std::vector<std::size_t> CACHE::get_rq_size() const
{
  std::vector<std::size_t> retval;
  std::transform(std::begin(upper_levels), std::end(upper_levels), std::back_inserter(retval), [](auto ulptr) { return ulptr->rq_size(); });
  return retval;
}

std::vector<std::size_t> CACHE::get_wq_size() const
{
  std::vector<std::size_t> retval;
  std::transform(std::begin(upper_levels), std::end(upper_levels), std::back_inserter(retval), [](auto ulptr) { return ulptr->wq_size(); });
  return retval;
}

std::vector<std::size_t> CACHE::get_pq_size() const
{
  std::vector<std::size_t> retval;
  std::transform(std::begin(upper_levels), std::end(upper_levels), std::back_inserter(retval), [](auto ulptr) { return ulptr->pq_size(); });
  retval.push_back(PQ_SIZE);
  return retval;
}

// LCOV_EXCL_START exclude deprecated function
std::size_t CACHE::get_size(uint8_t queue_type, uint64_t)
{
  if (queue_type == 0)
    return get_mshr_size();
  return 0;
}
// LCOV_EXCL_STOP

namespace
{
double occupancy_ratio(std::size_t occ, std::size_t sz) { return std::ceil(occ) / std::ceil(sz); }

std::vector<double> occupancy_ratio_vec(std::vector<std::size_t> occ, std::vector<std::size_t> sz)
{
  std::vector<double> retval;
  std::transform(std::begin(occ), std::end(occ), std::begin(sz), std::back_inserter(retval), occupancy_ratio);
  return retval;
}
} // namespace

double CACHE::get_mshr_occupancy_ratio() const { return ::occupancy_ratio(get_mshr_occupancy(), get_mshr_size()); }

std::vector<double> CACHE::get_rq_occupancy_ratio() const { return ::occupancy_ratio_vec(get_rq_occupancy(), get_rq_size()); }

std::vector<double> CACHE::get_wq_occupancy_ratio() const { return ::occupancy_ratio_vec(get_wq_occupancy(), get_wq_size()); }

std::vector<double> CACHE::get_pq_occupancy_ratio() const { return ::occupancy_ratio_vec(get_pq_occupancy(), get_pq_size()); }

void CACHE::initialize()
{
  impl_prefetcher_initialize();
  impl_initialize_replacement();
}

void CACHE::begin_phase()
{
  stats_type new_roi_stats, new_sim_stats;

  new_roi_stats.name = NAME;
  new_sim_stats.name = NAME;

  roi_stats = new_roi_stats;
  sim_stats = new_sim_stats;

  for (auto ul : upper_levels) {
    channel_type::stats_type ul_new_roi_stats, ul_new_sim_stats;
    ul->roi_stats = ul_new_roi_stats;
    ul->sim_stats = ul_new_sim_stats;
  }
}

void CACHE::end_phase(unsigned finished_cpu)
{
  auto total_miss = 0ull;
  for (auto type : {access_type::LOAD, access_type::RFO, access_type::PREFETCH, access_type::WRITE, access_type::TRANSLATION}) {
    total_miss =
        std::accumulate(std::begin(sim_stats.misses.at(champsim::to_underlying(type))), std::end(sim_stats.misses.at(champsim::to_underlying(type))), total_miss);
  }
  sim_stats.avg_miss_latency = std::ceil(sim_stats.total_miss_latency) / std::ceil(total_miss);

  roi_stats.total_miss_latency = sim_stats.total_miss_latency;
  roi_stats.avg_miss_latency = std::ceil(roi_stats.total_miss_latency) / std::ceil(total_miss);

  for (auto type : {access_type::LOAD, access_type::RFO, access_type::PREFETCH, access_type::WRITE, access_type::TRANSLATION}) {
    roi_stats.hits.at(champsim::to_underlying(type)).at(finished_cpu) = sim_stats.hits.at(champsim::to_underlying(type)).at(finished_cpu);
    roi_stats.misses.at(champsim::to_underlying(type)).at(finished_cpu) = sim_stats.misses.at(champsim::to_underlying(type)).at(finished_cpu);
  }

  roi_stats.pf_requested = sim_stats.pf_requested;
  roi_stats.pf_issued = sim_stats.pf_issued;
  roi_stats.pf_useful = sim_stats.pf_useful;
  roi_stats.pf_useless = sim_stats.pf_useless;
  roi_stats.pf_fill = sim_stats.pf_fill;

  for (auto ul : upper_levels) {
    ul->roi_stats.RQ_ACCESS = ul->sim_stats.RQ_ACCESS;
    ul->roi_stats.RQ_MERGED = ul->sim_stats.RQ_MERGED;
    ul->roi_stats.RQ_FULL = ul->sim_stats.RQ_FULL;
    ul->roi_stats.RQ_TO_CACHE = ul->sim_stats.RQ_TO_CACHE;

    ul->roi_stats.PQ_ACCESS = ul->sim_stats.PQ_ACCESS;
    ul->roi_stats.PQ_MERGED = ul->sim_stats.PQ_MERGED;
    ul->roi_stats.PQ_FULL = ul->sim_stats.PQ_FULL;
    ul->roi_stats.PQ_TO_CACHE = ul->sim_stats.PQ_TO_CACHE;

    ul->roi_stats.WQ_ACCESS = ul->sim_stats.WQ_ACCESS;
    ul->roi_stats.WQ_MERGED = ul->sim_stats.WQ_MERGED;
    ul->roi_stats.WQ_FULL = ul->sim_stats.WQ_FULL;
    ul->roi_stats.WQ_TO_CACHE = ul->sim_stats.WQ_TO_CACHE;
    ul->roi_stats.WQ_FORWARD = ul->sim_stats.WQ_FORWARD;
  }
}

template <typename T>
bool CACHE::should_activate_prefetcher(const T& pkt) const
{
  return ((1 << champsim::to_underlying(pkt.type)) & pref_activate_mask) && !pkt.prefetch_from_this;
}

// LCOV_EXCL_START Exclude the following function from LCOV
void CACHE::print_deadlock()
{
  std::string_view mshr_write{"instr_id: {} address: {:#x} v_addr: {:#x} type: {} event: {}"};
  auto mshr_pack = [](const auto& entry) {
    return std::tuple{entry.instr_id, entry.address, entry.v_address, access_type_names.at(champsim::to_underlying(entry.type)),
      entry.event_cycle};
  };

  std::string_view tag_check_write{"instr_id: {} address: {:#x} v_addr: {:#x} is_translated: {} translate_issued: {} event_cycle: {}"};
  auto tag_check_pack = [](const auto& entry) {
    return std::tuple{entry.instr_id, entry.address, entry.v_address, entry.is_translated, entry.translate_issued, entry.event_cycle};
  };

  champsim::range_print_deadlock(MSHR, NAME + "_MSHR", mshr_write, mshr_pack);
  champsim::range_print_deadlock(inflight_tag_check, NAME + "_tags", tag_check_write, tag_check_pack);
  champsim::range_print_deadlock(translation_stash, NAME + "_translation", tag_check_write, tag_check_pack);

  std::string_view q_writer{"instr_id: {} address: {:#x} v_addr: {:#x} type: {} translated: {}"};
  auto q_entry_pack = [](const auto& entry) {
    return std::tuple{entry.instr_id, entry.address, entry.v_address, access_type_names.at(champsim::to_underlying(entry.type)), entry.is_translated};
  };

  for (auto* ul : upper_levels) {
    champsim::range_print_deadlock(ul->RQ, NAME + "_RQ", q_writer, q_entry_pack);
    champsim::range_print_deadlock(ul->WQ, NAME + "_WQ", q_writer, q_entry_pack);
    champsim::range_print_deadlock(ul->PQ, NAME + "_PQ", q_writer, q_entry_pack);
  }
}
// LCOV_EXCL_STOP
