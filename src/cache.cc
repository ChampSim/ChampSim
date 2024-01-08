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

#include "bandwidth.h"
#include "champsim.h"
#include "champsim_constants.h"
#include "chrono.h"
#include "deadlock.h"
#include "instruction.h"
#include "util/algorithm.h"
#include "util/bits.h"
#include "util/span.h"

CACHE::tag_lookup_type::tag_lookup_type(const request_type& req, bool local_pref, bool skip)
    : address(req.address), v_address(req.v_address), data(req.data), ip(req.ip), instr_id(req.instr_id), pf_metadata(req.pf_metadata), cpu(req.cpu),
      type(req.type), prefetch_from_this(local_pref), skip_fill(skip), is_translated(req.is_translated), instr_depend_on_me(req.instr_depend_on_me)
{
}

CACHE::mshr_type::mshr_type(const tag_lookup_type& req, champsim::chrono::clock::time_point _time_enqueued)
    : address(req.address), v_address(req.v_address), ip(req.ip), instr_id(req.instr_id), cpu(req.cpu), type(req.type),
      prefetch_from_this(req.prefetch_from_this), time_enqueued(_time_enqueued), instr_depend_on_me(req.instr_depend_on_me), to_return(req.to_return)
{
}

CACHE::mshr_type CACHE::mshr_type::merge(mshr_type predecessor, mshr_type successor)
{
  std::vector<uint64_t> merged_instr{};
  std::vector<std::deque<response_type>*> merged_return{};

  std::set_union(std::begin(predecessor.instr_depend_on_me), std::end(predecessor.instr_depend_on_me), std::begin(successor.instr_depend_on_me),
                 std::end(successor.instr_depend_on_me), std::back_inserter(merged_instr));
  std::set_union(std::begin(predecessor.to_return), std::end(predecessor.to_return), std::begin(successor.to_return), std::end(successor.to_return),
                 std::back_inserter(merged_return));

  mshr_type retval{(successor.type == access_type::PREFETCH) ? predecessor : successor};
  retval.instr_depend_on_me = merged_instr;
  retval.to_return = merged_return;
  retval.data_promise = predecessor.data_promise;

  if constexpr (champsim::debug_print) {
    if (successor.type == access_type::PREFETCH) {
      fmt::print("[MSHR] {} address {:#x} type: {} into address {:#x} type: {}\n", __func__, successor.address,
                 access_type_names.at(champsim::to_underlying(successor.type)), predecessor.address,
                 access_type_names.at(champsim::to_underlying(successor.type)));
    } else {
      fmt::print("[MSHR] {} address {:#x} type: {} into address {:#x} type: {}\n", __func__, predecessor.address,
                 access_type_names.at(champsim::to_underlying(predecessor.type)), successor.address,
                 access_type_names.at(champsim::to_underlying(successor.type)));
    }
  }

  return retval;
}

auto CACHE::fill_block(mshr_type mshr, uint32_t metadata) -> BLOCK
{
  CACHE::BLOCK to_fill;
  to_fill.valid = true;
  to_fill.prefetch = mshr.prefetch_from_this;
  to_fill.dirty = (mshr.type == access_type::WRITE);
  to_fill.address = mshr.address;
  to_fill.v_address = mshr.v_address;
  to_fill.data = mshr.data_promise->data;
  to_fill.pf_metadata = metadata;

  return to_fill;
}

auto CACHE::matches_address(champsim::address addr) const
{
  return [match = addr.slice_upper(OFFSET_BITS), shamt = OFFSET_BITS](const auto& entry) {
    return entry.address.slice_upper(shamt) == match;
  };
}

template <typename T>
champsim::address CACHE::module_address(const T& element) const
{
  auto address = virtual_prefetch ? element.v_address : element.address;
  return champsim::address{address.slice_upper(match_offset_bits ? 0 : OFFSET_BITS)};
}

bool CACHE::handle_fill(const mshr_type& fill_mshr)
{
  cpu = fill_mshr.cpu;

  // find victim
  auto [set_begin, set_end] = get_set_span(fill_mshr.address);
  auto way = std::find_if_not(set_begin, set_end, [](auto x) { return x.valid; });
  if (way == set_end) {
    way = std::next(set_begin, impl_find_victim(fill_mshr.cpu, fill_mshr.instr_id, get_set_index(fill_mshr.address), &*set_begin, fill_mshr.ip,
                                                fill_mshr.address, fill_mshr.type));
  }
  assert(set_begin <= way);
  assert(way <= set_end);
  assert(way != set_end || fill_mshr.type != access_type::WRITE); // Writes may not bypass
  const auto way_idx = std::distance(set_begin, way);             // cast protected by earlier assertion

  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} instr_id: {} address: {} v_address: {} set: {} way: {} type: {} prefetch_metadata: {} cycle_enqueued: {} cycle: {}\n", NAME,
               __func__, fill_mshr.instr_id, fill_mshr.address, fill_mshr.v_address, get_set_index(fill_mshr.address), way_idx,
               access_type_names.at(champsim::to_underlying(fill_mshr.type)), fill_mshr.data_promise->pf_metadata,
               (fill_mshr.time_enqueued.time_since_epoch()) / clock_period, (current_time.time_since_epoch()) / clock_period);
  }

  if (way != set_end && way->valid && way->dirty) {
    request_type writeback_packet;

    writeback_packet.cpu = fill_mshr.cpu;
    writeback_packet.address = way->address;
    writeback_packet.data = way->data;
    writeback_packet.instr_id = fill_mshr.instr_id;
    writeback_packet.ip = champsim::address{};
    writeback_packet.type = access_type::WRITE;
    writeback_packet.pf_metadata = way->pf_metadata;
    writeback_packet.response_requested = false;

    if constexpr (champsim::debug_print) {
      fmt::print("[{}] {} evict address: {:#x} v_address: {:#x} prefetch_metadata: {}\n", NAME, __func__, writeback_packet.address, writeback_packet.v_address,
                 fill_mshr.data_promise->pf_metadata);
    }

    auto success = lower_level->add_wq(writeback_packet);
    if (!success) {
      return false;
    }
  }

  champsim::address evicting_address{};
  if (way != set_end && way->valid) {
    evicting_address = module_address(*way);
  }

  auto metadata_thru = impl_prefetcher_cache_fill(module_address(fill_mshr), get_set_index(fill_mshr.address), way_idx,
                                                  (fill_mshr.type == access_type::PREFETCH), evicting_address, fill_mshr.data_promise->pf_metadata);
  impl_update_replacement_state(fill_mshr.cpu, get_set_index(fill_mshr.address), way_idx, module_address(fill_mshr), fill_mshr.ip, evicting_address,
                                fill_mshr.type, false);

  if (way != set_end) {
    if (way->valid && way->prefetch) {
      ++sim_stats.pf_useless;
    }

    if (fill_mshr.type == access_type::PREFETCH) {
      ++sim_stats.pf_fill;
    }

    *way = fill_block(fill_mshr, metadata_thru);
  }

  // COLLECT STATS
  sim_stats.total_miss_latency += current_time - (fill_mshr.time_enqueued + clock_period);

  response_type response{fill_mshr.address, fill_mshr.v_address, fill_mshr.data_promise->data, metadata_thru, fill_mshr.instr_depend_on_me};
  for (auto* ret : fill_mshr.to_return) {
    ret->push_back(response);
  }

  return true;
}

bool CACHE::try_hit(const tag_lookup_type& handle_pkt)
{
  cpu = handle_pkt.cpu;

  // access cache
  auto [set_begin, set_end] = get_set_span(handle_pkt.address);
  auto way = std::find_if(set_begin, set_end, [matcher = matches_address(handle_pkt.address)](const auto& x) { return x.valid && matcher(x); });
  const auto hit = (way != set_end);
  const auto useful_prefetch = (hit && way->prefetch && !handle_pkt.prefetch_from_this);

  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} instr_id: {} address: {} v_address: {} data: {} set: {} way: {} ({}) type: {} cycle: {}\n", NAME, __func__,
               handle_pkt.instr_id, handle_pkt.address, handle_pkt.v_address, handle_pkt.data, get_set_index(handle_pkt.address), std::distance(set_begin, way),
               hit ? "HIT" : "MISS", access_type_names.at(champsim::to_underlying(handle_pkt.type)), current_time.time_since_epoch() / clock_period);
  }

  auto metadata_thru = handle_pkt.pf_metadata;
  if (should_activate_prefetcher(handle_pkt)) {
    metadata_thru = impl_prefetcher_cache_operate(module_address(handle_pkt), handle_pkt.ip, hit, useful_prefetch, handle_pkt.type, metadata_thru);
  }

  if (hit) {
    sim_stats.hits.increment(std::pair{handle_pkt.type, handle_pkt.cpu});

    // update replacement policy
    const auto way_idx = std::distance(set_begin, way);
    impl_update_replacement_state(handle_pkt.cpu, get_set_index(handle_pkt.address), way_idx, module_address(*way), handle_pkt.ip, champsim::address{},
                                  handle_pkt.type, true);

    response_type response{handle_pkt.address, handle_pkt.v_address, way->data, metadata_thru, handle_pkt.instr_depend_on_me};
    for (auto* ret : handle_pkt.to_return) {
      ret->push_back(response);
    }

    way->dirty |= (handle_pkt.type == access_type::WRITE);

    // update prefetch stats and reset prefetch bit
    if (useful_prefetch) {
      ++sim_stats.pf_useful;
      way->prefetch = false;
    }
  }

  return hit;
}

auto CACHE::mshr_and_forward_packet(const tag_lookup_type& handle_pkt) -> std::pair<mshr_type, request_type>
{
  mshr_type to_allocate{handle_pkt, current_time};

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

  return std::pair{std::move(to_allocate), std::move(fwd_pkt)};
}

bool CACHE::handle_miss(const tag_lookup_type& handle_pkt)
{
  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} instr_id: {} address: {} v_address: {} type: {} local_prefetch: {} cycle: {}\n", NAME, __func__, handle_pkt.instr_id,
               handle_pkt.address, handle_pkt.v_address, access_type_names.at(champsim::to_underlying(handle_pkt.type)), handle_pkt.prefetch_from_this,
               current_time.time_since_epoch() / clock_period);
  }

  mshr_type to_allocate{handle_pkt, current_time};

  cpu = handle_pkt.cpu;

  auto mshr_pkt = mshr_and_forward_packet(handle_pkt);

  // check mshr
  auto mshr_entry = std::find_if(std::begin(MSHR), std::end(MSHR), matches_address(handle_pkt.address));
  bool mshr_full = (MSHR.size() == MSHR_SIZE);

  if (mshr_entry != MSHR.end()) // miss already inflight
  {
    if (mshr_entry->type == access_type::PREFETCH && handle_pkt.type != access_type::PREFETCH) {
      // Mark the prefetch as useful
      if (mshr_entry->prefetch_from_this) {
        ++sim_stats.pf_useful;
      }
    }

    *mshr_entry = mshr_type::merge(*mshr_entry, to_allocate);
  } else {
    if (mshr_full) { // not enough MSHR resource
      return false;  // TODO should we allow prefetches anyway if they will not be filled to this level?
    }

    const bool send_to_rq = (prefetch_as_load || handle_pkt.type != access_type::PREFETCH);
    bool success = send_to_rq ? lower_level->add_rq(mshr_pkt.second) : lower_level->add_pq(mshr_pkt.second);

    if (!success) {
      return false;
    }

    // Allocate an MSHR
    if (mshr_pkt.second.response_requested) {
      MSHR.emplace_back(std::move(mshr_pkt.first));
    }
  }

  sim_stats.misses.increment(std::pair{handle_pkt.type, handle_pkt.cpu});

  return true;
}

bool CACHE::handle_write(const tag_lookup_type& handle_pkt)
{
  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} instr_id: {} address: {} v_address: {} type: {} local_prefetch: {} cycle: {}\n", NAME, __func__, handle_pkt.instr_id,
               handle_pkt.address, handle_pkt.v_address, access_type_names.at(champsim::to_underlying(handle_pkt.type)), handle_pkt.prefetch_from_this,
               current_time.time_since_epoch() / clock_period);
  }

  mshr_type to_allocate{handle_pkt, current_time};
  to_allocate.data_promise.ready_at(current_time + (warmup ? champsim::chrono::clock::duration{} : FILL_LATENCY));
  inflight_writes.push_back(to_allocate);

  sim_stats.misses.increment(std::pair{handle_pkt.type, handle_pkt.cpu});

  return true;
}

template <bool UpdateRequest>
auto CACHE::initiate_tag_check(champsim::channel* ul)
{
  return [time = current_time + (warmup ? champsim::chrono::clock::duration{} : HIT_LATENCY), ul](const auto& entry) {
    CACHE::tag_lookup_type retval{entry};
    retval.event_cycle = time;

    if constexpr (UpdateRequest) {
      if (entry.response_requested) {
        retval.to_return = {&ul->returned};
      }
    } else {
      (void)ul; // supress warning about ul being unused
    }

    if constexpr (champsim::debug_print) {
      fmt::print("[TAG] initiate_tag_check instr_id: {} address: {} v_address: {} type: {} response_requested: {} event: {}\n", retval.instr_id,
                 retval.address, retval.v_address, access_type_names.at(champsim::to_underlying(retval.type)), !std::empty(retval.to_return),
                 time);
    }

    return retval;
  };
}

long CACHE::operate()
{
  long progress{0};

  auto is_ready = [time = current_time](const auto& entry) {
    return entry.event_cycle <= time;
  };
  auto is_translated = [](const auto& entry) {
    return entry.is_translated;
  };

  for (auto* ul : upper_levels) {
    ul->check_collision();
  }

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
  champsim::bandwidth fill_bw{MAX_FILL};
  for (auto q : {std::ref(MSHR), std::ref(inflight_writes)}) {
    auto [fill_begin, fill_end] =
        champsim::get_span_p(std::cbegin(q.get()), std::cend(q.get()), fill_bw, [time = current_time](const auto& x) { return x.data_promise.is_ready_at(time); });
    auto complete_end = std::find_if_not(fill_begin, fill_end, [this](const auto& x) { return this->handle_fill(x); });
    fill_bw.consume(std::distance(fill_begin, complete_end));
    q.get().erase(fill_begin, complete_end);
  }

  // Initiate tag checks
  const champsim::bandwidth::maximum_type bandwidth_from_tag_checks{champsim::to_underlying(MAX_TAG) * (long)(HIT_LATENCY / clock_period) - (long)std::size(inflight_tag_check)};
  champsim::bandwidth initiate_tag_bw{std::clamp(bandwidth_from_tag_checks, champsim::bandwidth::maximum_type{0}, MAX_TAG)};
  auto can_translate = [avail = (std::size(translation_stash) < static_cast<std::size_t>(MSHR_SIZE))](const auto& entry) {
    return avail || entry.is_translated;
  };
  auto stash_bandwidth_consumed =
      champsim::transform_while_n(translation_stash, std::back_inserter(inflight_tag_check), initiate_tag_bw, is_translated, initiate_tag_check<false>());
  initiate_tag_bw.consume(stash_bandwidth_consumed);
  std::vector<long long> channels_bandwidth_consumed{};
  for (auto* ul : upper_levels) {
    for (auto q : {std::ref(ul->WQ), std::ref(ul->RQ), std::ref(ul->PQ)}) {
      auto bandwidth_consumed =
          champsim::transform_while_n(q.get(), std::back_inserter(inflight_tag_check), initiate_tag_bw, can_translate, initiate_tag_check<true>(ul));
      channels_bandwidth_consumed.push_back(bandwidth_consumed);
      initiate_tag_bw.consume(bandwidth_consumed);
    }
  }
  auto pq_bandwidth_consumed =
      champsim::transform_while_n(internal_PQ, std::back_inserter(inflight_tag_check), initiate_tag_bw, can_translate, initiate_tag_check<false>());
  initiate_tag_bw.consume(pq_bandwidth_consumed);

  // Issue translations
  std::for_each(std::begin(inflight_tag_check), std::end(inflight_tag_check), [this](auto& x) { this->issue_translation(x); });
  std::for_each(std::begin(translation_stash), std::end(translation_stash), [this](auto& x) { this->issue_translation(x); });

  // Find entries that would be ready except that they have not finished translation, move them to the stash
  auto [last_not_missed, stash_end] = champsim::extract_if(std::begin(inflight_tag_check), std::end(inflight_tag_check), std::back_inserter(translation_stash),
                                                           [is_ready, is_translated](const auto& x) { return is_ready(x) && !is_translated(x); });
  progress += std::distance(last_not_missed, std::end(inflight_tag_check));
  inflight_tag_check.erase(last_not_missed, std::end(inflight_tag_check));

  // Perform tag checks
  auto do_handle_miss = [this](const auto& pkt) {
    if (pkt.type == access_type::WRITE && !this->match_offset_bits) {
      return this->handle_write(pkt); // Treat writes (that is, writebacks) like fills
    }
    return this->handle_miss(pkt); // Treat writes (that is, stores) like reads
  };
  champsim::bandwidth tag_check_bw{MAX_TAG};
  auto [tag_check_ready_begin, tag_check_ready_end] =
      champsim::get_span_p(std::begin(inflight_tag_check), std::end(inflight_tag_check), tag_check_bw,
                           [is_ready, is_translated](const auto& pkt) { return is_ready(pkt) && is_translated(pkt); });
  auto hits_end = std::stable_partition(tag_check_ready_begin, tag_check_ready_end, [this](const auto& pkt) { return this->try_hit(pkt); });
  auto finish_tag_check_end = std::stable_partition(hits_end, tag_check_ready_end, do_handle_miss);
  tag_check_bw.consume(std::distance(tag_check_ready_begin, finish_tag_check_end));
  inflight_tag_check.erase(tag_check_ready_begin, finish_tag_check_end);

  impl_prefetcher_cycle_operate();

  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} cycle completed: {} tags checked: {} remaining: {} stash consumed: {} remaining: {} channel consumed: {} pq consumed {} unused consume "
               "bw {}\n",
               NAME, __func__, current_time.time_since_epoch() / clock_period, tag_check_bw.amount_consumed(), std::size(inflight_tag_check), stash_bandwidth_consumed,
               std::size(translation_stash), channels_bandwidth_consumed, pq_bandwidth_consumed, initiate_tag_bw.amount_remaining());
  }

  return progress + fill_bw.amount_consumed() + initiate_tag_bw.amount_consumed() + tag_check_bw.amount_consumed();
}

// LCOV_EXCL_START exclude deprecated function
uint64_t CACHE::get_set(uint64_t address) const { return static_cast<uint64_t>(get_set_index(champsim::address{address})); }
// LCOV_EXCL_STOP

long CACHE::get_set_index(champsim::address address) const { return address.slice(champsim::sized_extent{OFFSET_BITS, champsim::lg2(NUM_SET)}).to<long>(); }

template <typename It>
std::pair<It, It> get_span(It anchor, typename std::iterator_traits<It>::difference_type set_idx, typename std::iterator_traits<It>::difference_type num_way)
{
  auto begin = std::next(anchor, set_idx * num_way);
  return {std::move(begin), std::next(begin, num_way)};
}

auto CACHE::get_set_span(champsim::address address) -> std::pair<set_type::iterator, set_type::iterator>
{
  const auto set_idx = get_set_index(address);
  assert(set_idx < NUM_SET);
  return get_span(std::begin(block), static_cast<set_type::difference_type>(set_idx), NUM_WAY); // safe cast because of prior assert
}

auto CACHE::get_set_span(champsim::address address) const -> std::pair<set_type::const_iterator, set_type::const_iterator>
{
  const auto set_idx = get_set_index(address);
  assert(set_idx < NUM_SET);
  return get_span(std::cbegin(block), static_cast<set_type::difference_type>(set_idx), NUM_WAY); // safe cast because of prior assert
}

// LCOV_EXCL_START exclude deprecated function
uint64_t CACHE::get_way(uint64_t address, uint64_t /*unused set index*/) const
{
  champsim::address intern_addr{address};
  auto [begin, end] = get_set_span(intern_addr);
  return static_cast<uint64_t>(std::distance(begin, std::find_if(begin, end, matches_address(champsim::address{address}))));
}
// LCOV_EXCL_STOP

long CACHE::invalidate_entry(champsim::address inval_addr)
{
  auto [begin, end] = get_set_span(inval_addr);
  auto inv_way = std::find_if(begin, end, matches_address(inval_addr));

  if (inv_way != end) {
    inv_way->valid = false;
  }

  return std::distance(begin, inv_way);
}

bool CACHE::prefetch_line(champsim::address pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  ++sim_stats.pf_requested;

  if (std::size(internal_PQ) >= PQ_SIZE) {
    return false;
  }

  request_type pf_packet;
  pf_packet.type = access_type::PREFETCH;
  pf_packet.pf_metadata = prefetch_metadata;
  pf_packet.cpu = cpu;
  pf_packet.address = pf_addr;
  pf_packet.v_address = virtual_prefetch ? pf_addr : champsim::address{};
  pf_packet.is_translated = !virtual_prefetch;

  internal_PQ.emplace_back(pf_packet, true, !fill_this_level);
  ++sim_stats.pf_issued;

  return true;
}

// LCOV_EXCL_START exclude deprecated function
bool CACHE::prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  return prefetch_line(champsim::address{pf_addr}, fill_this_level, prefetch_metadata);
}

bool CACHE::prefetch_line(uint64_t /*deprecated*/, uint64_t /*deprecated*/, uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  return prefetch_line(champsim::address{pf_addr}, fill_this_level, prefetch_metadata);
}
// LCOV_EXCL_STOP

void CACHE::finish_packet(const response_type& packet)
{
  // check MSHR information
  auto mshr_entry = std::find_if(std::begin(MSHR), std::end(MSHR), matches_address(packet.address));
  auto first_unreturned = std::find_if(MSHR.begin(), MSHR.end(), [](auto x) { return x.data_promise.has_unknown_readiness(); });

  // sanity check
  if (mshr_entry == MSHR.end()) {
    fmt::print(stderr, "[{}_MSHR] {} cannot find a matching entry! address: {} v_address: {}\n", NAME, __func__, packet.address, packet.v_address);
    assert(0);
  }

  // MSHR holds the most updated information about this request
  mshr_type::returned_value finished_value{packet.data, packet.pf_metadata};
  mshr_entry->data_promise = champsim::waitable{finished_value, current_time + (warmup ? champsim::chrono::clock::duration{} : FILL_LATENCY)};
  if constexpr (champsim::debug_print) {
    fmt::print("[{}_MSHR] finish_packet instr_id: {} address: {} data: {} type: {} current: {}\n", this->NAME, mshr_entry->instr_id, mshr_entry->address,
               mshr_entry->data_promise->data, access_type_names.at(champsim::to_underlying(mshr_entry->type)), current_time.time_since_epoch() / clock_period);
  }

  // Order this entry after previously-returned entries, but before non-returned
  // entries
  std::iter_swap(mshr_entry, first_unreturned);
}

void CACHE::finish_translation(const response_type& packet)
{
  auto matches_vpage = [page_num = champsim::page_number{packet.v_address}](const auto& entry) {
    return champsim::page_number{entry.v_address} == page_num;
  };
  auto mark_translated = [p_page = champsim::page_number{packet.data}, this](auto& entry) {
    entry.address = champsim::splice(p_page, champsim::page_offset{entry.v_address}); // translated address
    entry.is_translated = true;                                                       // This entry is now translated

    if constexpr (champsim::debug_print) {
      fmt::print("[{}_TRANSLATE] finish_translation paddr: {} vaddr: {} cycle: {}\n", this->NAME, entry.address, entry.v_address,
          this->current_time.time_since_epoch() / this->clock_period);
    }
  };

  // Restart stashed translations
  auto finish_begin = std::find_if_not(std::begin(translation_stash), std::end(translation_stash), [](const auto& x) { return x.is_translated; });
  auto finish_end = std::stable_partition(finish_begin, std::end(translation_stash), matches_vpage);
  std::for_each(finish_begin, finish_end, mark_translated);

  // Find all packets that match the page of the returned packet
  for (auto& entry : inflight_tag_check) {
    if (matches_vpage(entry)) {
      mark_translated(entry);
    }
  }
}

void CACHE::issue_translation(tag_lookup_type& q_entry) const
{
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

    q_entry.translate_issued = lower_translate->add_rq(fwd_pkt);
    if constexpr (champsim::debug_print) {
      if (q_entry.translate_issued) {
        fmt::print("[TRANSLATE] do_issue_translation instr_id: {} paddr: {} vaddr: {} type: {}\n", q_entry.instr_id, q_entry.address, q_entry.v_address,
                   access_type_names.at(champsim::to_underlying(q_entry.type)));
      }
    }
  }
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
std::size_t CACHE::get_occupancy(uint8_t queue_type, uint64_t /*deprecated*/) const
{
  if (queue_type == 0) {
    return get_mshr_occupancy();
  }
  return 0;
}

std::size_t CACHE::get_occupancy(uint8_t queue_type, champsim::address /*deprecated*/) const
{
  if (queue_type == 0) {
    return get_mshr_occupancy();
  }
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
std::size_t CACHE::get_size(uint8_t queue_type, champsim::address /*deprecated*/) const
{
  if (queue_type == 0) {
    return get_mshr_size();
  }
  return 0;
}

std::size_t CACHE::get_size(uint8_t queue_type, uint64_t /*deprecated*/) const
{
  if (queue_type == 0) {
    return get_mshr_size();
  }
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

void CACHE::impl_prefetcher_initialize() const { pref_module_pimpl->impl_prefetcher_initialize(); }

uint32_t CACHE::impl_prefetcher_cache_operate(champsim::address addr, champsim::address ip, bool cache_hit, bool useful_prefetch, access_type type,
                                              uint32_t metadata_in) const
{
  return pref_module_pimpl->impl_prefetcher_cache_operate(addr, ip, cache_hit, useful_prefetch, type, metadata_in);
}

uint32_t CACHE::impl_prefetcher_cache_fill(champsim::address addr, long set, long way, bool prefetch, champsim::address evicted_addr,
                                           uint32_t metadata_in) const
{
  return pref_module_pimpl->impl_prefetcher_cache_fill(addr, set, way, prefetch, evicted_addr, metadata_in);
}

void CACHE::impl_prefetcher_cycle_operate() const { pref_module_pimpl->impl_prefetcher_cycle_operate(); }

void CACHE::impl_prefetcher_final_stats() const { pref_module_pimpl->impl_prefetcher_final_stats(); }

void CACHE::impl_prefetcher_branch_operate(champsim::address ip, uint8_t branch_type, champsim::address branch_target) const
{
  pref_module_pimpl->impl_prefetcher_branch_operate(ip, branch_type, branch_target);
}

void CACHE::impl_initialize_replacement() const { repl_module_pimpl->impl_initialize_replacement(); }

long CACHE::impl_find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const BLOCK* current_set, champsim::address ip, champsim::address full_addr,
                             access_type type) const
{
  return repl_module_pimpl->impl_find_victim(triggering_cpu, instr_id, set, current_set, ip, full_addr, type);
}

void CACHE::impl_update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip,
                                          champsim::address victim_addr, access_type type, bool hit) const
{
  repl_module_pimpl->impl_update_replacement_state(triggering_cpu, set, way, full_addr, ip, victim_addr, type, hit);
}

void CACHE::impl_replacement_final_stats() const { repl_module_pimpl->impl_replacement_final_stats(); }

void CACHE::initialize()
{
  impl_prefetcher_initialize();
  impl_initialize_replacement();
}

void CACHE::begin_phase()
{
  stats_type new_roi_stats;
  stats_type new_sim_stats;

  new_roi_stats.name = NAME;
  new_sim_stats.name = NAME;

  roi_stats = new_roi_stats;
  sim_stats = new_sim_stats;

  for (auto* ul : upper_levels) {
    channel_type::stats_type ul_new_roi_stats;
    channel_type::stats_type ul_new_sim_stats;
    ul->roi_stats = ul_new_roi_stats;
    ul->sim_stats = ul_new_sim_stats;
  }
}

void CACHE::end_phase(unsigned finished_cpu)
{
  sim_stats.avg_miss_latency = sim_stats.total_miss_latency / std::ceil(sim_stats.misses.total()) / clock_period;

  roi_stats.total_miss_latency = sim_stats.total_miss_latency;
  roi_stats.avg_miss_latency = roi_stats.total_miss_latency / std::ceil(sim_stats.misses.total()) / clock_period;

  for (auto type : {access_type::LOAD, access_type::RFO, access_type::PREFETCH, access_type::WRITE, access_type::TRANSLATION}) {
    std::pair key{type, finished_cpu};
    roi_stats.hits.set(key, sim_stats.hits.value_or(key, 0));
    roi_stats.misses.set(key, sim_stats.misses.value_or(key, 0));
  }

  roi_stats.pf_requested = sim_stats.pf_requested;
  roi_stats.pf_issued = sim_stats.pf_issued;
  roi_stats.pf_useful = sim_stats.pf_useful;
  roi_stats.pf_useless = sim_stats.pf_useless;
  roi_stats.pf_fill = sim_stats.pf_fill;

  for (auto* ul : upper_levels) {
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
  return !pkt.prefetch_from_this && std::count(std::begin(pref_activate_mask), std::end(pref_activate_mask), pkt.type) > 0;
}

// LCOV_EXCL_START Exclude the following function from LCOV
void CACHE::print_deadlock()
{
  std::string_view mshr_write{"instr_id: {} address: {} v_addr: {} type: {} ready: {}"};
  auto mshr_pack = [time = current_time](const auto& entry) {
    return std::tuple{entry.instr_id, entry.address, entry.v_address, access_type_names.at(champsim::to_underlying(entry.type)),
                      entry.data_promise.is_ready_at(time)};
  };

  std::string_view tag_check_write{"instr_id: {} address: {} v_addr: {} is_translated: {} translate_issued: {} event_cycle: {}"};
  auto tag_check_pack = [period = clock_period](const auto& entry) {
    return std::tuple{entry.instr_id, entry.address, entry.v_address, entry.is_translated, entry.translate_issued, entry.event_cycle.time_since_epoch() / period};
  };

  champsim::range_print_deadlock(MSHR, NAME + "_MSHR", mshr_write, mshr_pack);
  champsim::range_print_deadlock(inflight_tag_check, NAME + "_tags", tag_check_write, tag_check_pack);
  champsim::range_print_deadlock(translation_stash, NAME + "_translation", tag_check_write, tag_check_pack);

  std::string_view q_writer{"instr_id: {} address: {} v_addr: {} type: {} translated: {}"};
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
