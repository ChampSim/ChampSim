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

#include "champsim.h"
#include "champsim_constants.h"
#include "instruction.h"
#include "util/bits.h"
#include "util/span.h"

using namespace std::literals::string_view_literals;
constexpr std::array<std::string_view, NUM_TYPES> access_type_names{"LOAD"sv, "RFO"sv, "PREFETCH"sv, "WRITE"sv, "TRANSLATION"};

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

CACHE::BLOCK::BLOCK(mshr_type mshr, uint32_t metadata)
    : valid(true), prefetch(mshr.prefetch_from_this), dirty(mshr.type == WRITE), address(mshr.address), v_address(mshr.v_address), data(mshr.data), pf_metadata(metadata)
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
                                                fill_mshr.address, fill_mshr.type));
  assert(set_begin <= way);
  assert(way <= set_end);
  const auto way_idx = std::distance(set_begin, way);

  if constexpr (champsim::debug_print) {
    std::cout << "[" << NAME << "] " << __func__;
    std::cout << " instr_id: " << fill_mshr.instr_id;
    std::cout << " full_addr: " << fill_mshr.address;
    std::cout << " full_v_addr: " << fill_mshr.v_address;
    std::cout << " set: " << get_set_index(fill_mshr.address);
    std::cout << " way: " << way_idx;
    std::cout << " type: " << access_type_names.at(fill_mshr.type);
    std::cout << " prefetch_metadata: " << fill_mshr.pf_metadata;
    std::cout << " cycle_enqueued: " << fill_mshr.cycle_enqueued;
    std::cout << " cycle: " << current_cycle << std::endl;
  }

  bool success = true;
  auto metadata_thru = fill_mshr.pf_metadata;
  champsim::address pkt_address{(virtual_prefetch ? fill_mshr.v_address : fill_mshr.address).slice_upper(match_offset_bits ? 0 : OFFSET_BITS)};
  if (way != set_end) {
    if (way->valid && way->dirty) {
      request_type writeback_packet;

      writeback_packet.cpu = fill_mshr.cpu;
      writeback_packet.address = way->address;
      writeback_packet.data = way->data;
      writeback_packet.instr_id = fill_mshr.instr_id;
      writeback_packet.ip = champsim::address{};
      writeback_packet.type = WRITE;
      writeback_packet.pf_metadata = way->pf_metadata;
      writeback_packet.response_requested = false;

      success = lower_level->add_wq(writeback_packet);
    }

    if (success) {
      champsim::address evicting_address{(ever_seen_data ? way->address : way->v_address).slice_upper(match_offset_bits ? 0 : OFFSET_BITS)};

      if (way->prefetch)
        ++sim_stats.pf_useless;

      if (fill_mshr.type == PREFETCH)
        ++sim_stats.pf_fill;

      metadata_thru =
          impl_prefetcher_cache_fill(pkt_address, get_set_index(fill_mshr.address), way_idx, fill_mshr.type == PREFETCH, evicting_address, metadata_thru);
      impl_update_replacement_state(fill_mshr.cpu, get_set_index(fill_mshr.address), way_idx, fill_mshr.address, fill_mshr.ip, evicting_address, fill_mshr.type,
                                    false);

      *way = BLOCK{fill_mshr, metadata_thru};
    }
  } else {
    // Bypass
    assert(fill_mshr.type != WRITE);

    metadata_thru = impl_prefetcher_cache_fill(pkt_address, get_set_index(fill_mshr.address), way_idx, fill_mshr.type == PREFETCH, champsim::address{}, metadata_thru);
    impl_update_replacement_state(fill_mshr.cpu, get_set_index(fill_mshr.address), way_idx, fill_mshr.address, fill_mshr.ip, champsim::address{}, fill_mshr.type, false);
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
  auto way = std::find_if(set_begin, set_end, [match=handle_pkt.address.slice_upper(OFFSET_BITS), offset=OFFSET_BITS](const auto& x){ return x.valid && x.address.slice_upper(offset) == match; });
  const auto hit = (way != set_end);

  if constexpr (champsim::debug_print) {
    std::cout << "[" << NAME << "] " << __func__;
    std::cout << " instr_id: " << handle_pkt.instr_id;
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address;
    std::cout << " set: " << get_set_index(handle_pkt.address);
    std::cout << " way: " << std::distance(set_begin, way) << " (" << (hit ? "HIT" : "MISS") << ")";
    std::cout << " type: " << access_type_names.at(handle_pkt.type);
    std::cout << " cycle: " << current_cycle << std::endl;
  }

  // update prefetcher on load instructions and prefetches from upper levels
  auto metadata_thru = handle_pkt.pf_metadata;
  if (should_activate_prefetcher(handle_pkt)) {
    champsim::address pf_base_addr{ (virtual_prefetch ? handle_pkt.v_address : handle_pkt.address).slice_upper(match_offset_bits ? 0 : OFFSET_BITS)};
    metadata_thru = impl_prefetcher_cache_operate(pf_base_addr, handle_pkt.ip, hit, handle_pkt.type, metadata_thru);
  }

  if (hit) {
    ++sim_stats.hits[handle_pkt.type][handle_pkt.cpu];

    // update replacement policy
    const auto way_idx = std::distance(set_begin, way);
    impl_update_replacement_state(handle_pkt.cpu, get_set_index(handle_pkt.address), way_idx, way->address, handle_pkt.ip, champsim::address{}, handle_pkt.type, true);

    response_type response{handle_pkt.address, handle_pkt.v_address, way->data, metadata_thru, handle_pkt.instr_depend_on_me};
    for (auto ret : handle_pkt.to_return)
      ret->push_back(response);

    way->dirty = (handle_pkt.type == WRITE);

    // update prefetch stats and reset prefetch bit
    if (way->prefetch && !handle_pkt.prefetch_from_this) {
      ++sim_stats.pf_useful;
      way->prefetch = false;
    }
  } else {
    ++sim_stats.misses[handle_pkt.type][handle_pkt.cpu];
  }

  return hit;
}

bool CACHE::handle_miss(const tag_lookup_type& handle_pkt)
{
  if constexpr (champsim::debug_print) {
    std::cout << "[" << NAME << "] " << __func__;
    std::cout << " instr_id: " << handle_pkt.instr_id;
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address;
    std::cout << " type: " << access_type_names.at(handle_pkt.type);
    std::cout << " local_prefetch: " << std::boolalpha << handle_pkt.prefetch_from_this;
    std::cout << " create mshr?: " << !handle_pkt.skip_fill << std::noboolalpha;
    std::cout << " cycle: " << current_cycle << std::endl;
  }

  cpu = handle_pkt.cpu;

  // check mshr
  auto mshr_entry = std::find_if(std::begin(MSHR), std::end(MSHR), [match = handle_pkt.address.slice_upper(OFFSET_BITS), offset = OFFSET_BITS](const auto& mshr) {
    return mshr.address.slice_upper(offset) == match;
  });
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
        ++sim_stats.pf_useful;

      uint64_t prior_event_cycle = mshr_entry->event_cycle;
      auto to_return = std::move(mshr_entry->to_return);
      *mshr_entry = mshr_type{handle_pkt, current_cycle};

      // in case request is already returned, we should keep event_cycle
      mshr_entry->event_cycle = prior_event_cycle;
      mshr_entry->to_return = std::move(to_return);
    }
  } else {
    if (mshr_full)  // not enough MSHR resource
      return false; // TODO should we allow prefetches anyway if they will not be filled to this level?

    request_type fwd_pkt;

    fwd_pkt.asid[0] = handle_pkt.asid[0];
    fwd_pkt.asid[1] = handle_pkt.asid[1];
    fwd_pkt.type = (handle_pkt.type == WRITE) ? RFO : handle_pkt.type;
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
    if (prefetch_as_load || handle_pkt.type != PREFETCH)
      success = lower_level->add_rq(fwd_pkt);
    else
      success = lower_level->add_pq(fwd_pkt);

    if (!success)
      return false;

    // Allocate an MSHR
    if (fwd_pkt.response_requested) {
      MSHR.emplace_back(handle_pkt, current_cycle);
      MSHR.back().pf_metadata = fwd_pkt.pf_metadata;
    }
  }

  return true;
}

bool CACHE::handle_write(const tag_lookup_type& handle_pkt)
{
  if constexpr (champsim::debug_print) {
    std::cout << "[" << NAME << "] " << __func__;
    std::cout << " instr_id: " << handle_pkt.instr_id;
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address;
    std::cout << " type: " << access_type_names.at(handle_pkt.type);
    std::cout << " local_prefetch: " << std::boolalpha << handle_pkt.prefetch_from_this << std::noboolalpha;
    std::cout << " cycle: " << current_cycle << std::endl;
  }

  inflight_writes.emplace_back(handle_pkt, current_cycle);
  inflight_writes.back().event_cycle = current_cycle + (warmup ? 0 : FILL_LATENCY);

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
  for (auto ul : upper_levels)
    ul->check_collision();

  // Finish returns
  std::for_each(std::cbegin(lower_level->returned), std::cend(lower_level->returned), [this](const auto& pkt) { this->finish_packet(pkt); });
  lower_level->returned.clear();

  // Finish translations
  if (lower_translate != nullptr) {
    std::for_each(std::cbegin(lower_translate->returned), std::cend(lower_translate->returned), [this](const auto& pkt) { this->finish_translation(pkt); });
    lower_translate->returned.clear();
  }

  // Perform fills
  auto fill_bw = MAX_FILL;
  for (auto q : {std::ref(MSHR), std::ref(inflight_writes)}) {
    fill_bw -= operate_queue(q.get(), fill_bw, [cycle = current_cycle, this](const auto& x) { return x.event_cycle <= cycle && this->handle_fill(x); });
  }

  // Initiate tag checks
  auto tag_bw = MAX_TAG;
  for (auto ul : upper_levels) {
    for (auto q : {std::ref(ul->WQ), std::ref(ul->RQ), std::ref(ul->PQ)}) {
      tag_bw -= operate_queue(q.get(), tag_bw, [cycle = current_cycle + (warmup ? 0 : HIT_LATENCY), ul, this](const auto& entry) {
        tag_lookup_type retval{entry};
        retval.event_cycle = cycle;
        if (entry.response_requested)
          retval.to_return = {&ul->returned};
        this->inflight_tag_check.push_back(retval);
        return true;
      });
    }
  }
  tag_bw -= operate_queue(internal_PQ, tag_bw, [cycle = current_cycle + (warmup ? 0 : HIT_LATENCY), this](const auto& entry) {
    tag_lookup_type retval{entry};
    retval.event_cycle = cycle;
    this->inflight_tag_check.push_back(retval);
    return true;
  });

  // Issue translations
  issue_translation();

  // Detect translations that have missed
  detect_misses();

  // Perform tag checks
  operate_queue(inflight_tag_check, MAX_TAG, [cycle = current_cycle, this](const auto& pkt) {
    return pkt.event_cycle <= cycle && pkt.is_translated
           && (this->try_hit(pkt)
               || ((pkt.type == WRITE && !this->match_offset_bits) ? this->handle_write(pkt) // Treat writes (that is, writebacks) like fills
                                                                   : this->handle_miss(pkt)  // Treat writes (that is, stores) like reads
                   ));
  });

  impl_prefetcher_cycle_operate();
}

uint64_t CACHE::get_set(uint64_t address) const { return get_set_index(champsim::address{address}); }

std::size_t CACHE::get_set_index(champsim::address address) const { return address.slice(champsim::lg2(NUM_SET)+OFFSET_BITS, OFFSET_BITS).to<std::size_t>(); }

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

uint64_t CACHE::get_way(uint64_t address, uint64_t) const
{
  champsim::address intern_addr{address};
  auto [begin, end] = get_set_span(intern_addr);
  return std::distance(begin, std::find_if(begin, end, [match=intern_addr.slice_upper(OFFSET_BITS), offset=OFFSET_BITS](const auto& blk){ return blk.valid && blk.address.slice_upper(offset) == match; }));
}

uint64_t CACHE::invalidate_entry(champsim::address inval_addr)
{
  auto [begin, end] = get_set_span(inval_addr);
  auto inv_way = std::find_if(begin, end, [match=inval_addr.slice_upper(OFFSET_BITS), offset=OFFSET_BITS](const auto& blk){ return blk.valid && blk.address.slice_upper(offset) == match; });

  if (inv_way != end)
    inv_way->valid = 0;

  return std::distance(begin, inv_way);
}

int CACHE::prefetch_line(champsim::address pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  ++sim_stats.pf_requested;

  if (std::size(internal_PQ) >= PQ_SIZE)
    return false;

  request_type pf_packet;
  pf_packet.type = PREFETCH;
  pf_packet.pf_metadata = prefetch_metadata;
  pf_packet.cpu = cpu;
  pf_packet.address = pf_addr;
  pf_packet.v_address = virtual_prefetch ? pf_addr : champsim::address{};
  pf_packet.is_translated = !virtual_prefetch;

  internal_PQ.emplace_back(pf_packet, true, !fill_this_level);
  ++sim_stats.pf_issued;

  return true;
}

int CACHE::prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  return prefetch_line(champsim::address{pf_addr}, fill_this_level, prefetch_metadata);
}

int CACHE::prefetch_line(uint64_t, uint64_t, uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  return prefetch_line(champsim::address{pf_addr}, fill_this_level, prefetch_metadata);
}

void CACHE::finish_packet(const response_type& packet)
{
  // check MSHR information
  auto mshr_entry = std::find_if(std::begin(MSHR), std::end(MSHR),
      [match = packet.address.slice_upper(OFFSET_BITS), offset = OFFSET_BITS](const auto& x) { return x.address.slice_upper(offset) == match; });
  auto first_unreturned = std::find_if(MSHR.begin(), MSHR.end(), [](auto x) { return x.event_cycle == std::numeric_limits<uint64_t>::max(); });

  // sanity check
  if (mshr_entry == MSHR.end()) {
    std::cerr << "[" << NAME << "_MSHR] " << __func__ << " cannot find a matching entry!";
    std::cerr << " address: " << packet.address;
    std::cerr << " v_address: " << packet.v_address;
    std::cerr << " current: " << current_cycle << std::endl;
    assert(0);
  }

  // MSHR holds the most updated information about this request
  mshr_entry->data = packet.data;
  mshr_entry->pf_metadata = packet.pf_metadata;
  mshr_entry->event_cycle = current_cycle + (warmup ? 0 : FILL_LATENCY);

  if constexpr (champsim::debug_print) {
    std::cout << "[" << NAME << "_MSHR] " << __func__;
    std::cout << " instr_id: " << mshr_entry->instr_id;
    std::cout << " address: " << mshr_entry->address;
    std::cout << " full_v_addr: " << mshr_entry->v_address;
    std::cout << " data: " << mshr_entry->data;
    std::cout << " type: " << access_type_names.at(mshr_entry->type);
    std::cout << " to_finish: " << std::size(lower_level->returned);
    std::cout << " event: " << mshr_entry->event_cycle << " current: " << current_cycle << std::endl;
  }

  // Order this entry after previously-returned entries, but before non-returned
  // entries
  std::iter_swap(mshr_entry, first_unreturned);
}

void CACHE::finish_translation(const response_type& packet)
{
  // Find all packets that match the page of the returned packet
  for (auto& entry : inflight_tag_check) {
    if (champsim::page_number{entry.v_address} == champsim::page_number{packet.v_address}) {
      entry.address = champsim::splice(champsim::page_number{packet.data}, champsim::page_offset{entry.v_address}); // translated address
      entry.is_translated = true;                                                          // This entry is now translated

      if constexpr (champsim::debug_print) {
        std::cout << "[" << NAME << "_TRANSLATE] " << __func__;
        std::cout << " paddr: " << entry.address;
        std::cout << " vaddr: " << entry.v_address;
        std::cout << " cycle: " << current_cycle << std::endl;
      }
    }
  }

  auto stash_it =
      std::stable_partition(std::begin(translation_stash), std::end(translation_stash),
                            [page_num = champsim::page_number{packet.v_address}](const auto& entry) { return champsim::page_number{entry.v_address} != page_num; });
  auto tag_check_it = inflight_tag_check.insert(std::cend(inflight_tag_check), stash_it, std::end(translation_stash));
  translation_stash.erase(stash_it, std::end(translation_stash));
  std::for_each(tag_check_it, std::end(inflight_tag_check), [cycle = current_cycle + (warmup ? 0 : HIT_LATENCY), addr = packet.data, this](auto& entry) {
    entry.address = champsim::splice(champsim::page_number{addr}, champsim::page_offset{entry.v_address}); // translated address
    entry.event_cycle = cycle;
    entry.is_translated = true; // This entry is now translated

    if constexpr (champsim::debug_print) {
      std::cout << "[" << this->NAME << "_TRANSLATE] " << __func__;
      std::cout << " paddr: " << entry.address;
      std::cout << " vaddr: " << entry.v_address;
      std::cout << " cycle: " << this->current_cycle << std::endl;
    }
  });
}

void CACHE::issue_translation()
{
  std::for_each(std::begin(inflight_tag_check), std::end(inflight_tag_check), [this](auto& q_entry) {
    if (!q_entry.translate_issued && !q_entry.is_translated && q_entry.address == q_entry.v_address) {
      request_type fwd_pkt;
      fwd_pkt.asid[0] = q_entry.asid[0];
      fwd_pkt.asid[1] = q_entry.asid[1];
      fwd_pkt.type = LOAD;
      fwd_pkt.cpu = q_entry.cpu;

      fwd_pkt.address = q_entry.address;
      fwd_pkt.v_address = q_entry.v_address;
      fwd_pkt.data = q_entry.data;
      fwd_pkt.instr_id = q_entry.instr_id;
      fwd_pkt.ip = q_entry.ip;

      fwd_pkt.instr_depend_on_me = q_entry.instr_depend_on_me;
      fwd_pkt.is_translated = true;

      auto success = this->lower_translate->add_rq(fwd_pkt);
      if (success) {
        if constexpr (champsim::debug_print) {
          std::cout << "[TRANSLATE] do_issue_translation instr_id: " << q_entry.instr_id;
          std::cout << " address: " << q_entry.address << " v_address: " << q_entry.v_address;
          std::cout << " type: " << access_type_names.at(q_entry.type) << std::endl;
        }

        q_entry.translate_issued = true;
        q_entry.address = champsim::address{};
      }
    }
  });
}

void CACHE::detect_misses()
{
  // Find entries that would be ready except that they have not finished translation
  auto missed = [cycle = current_cycle](auto x) {
    return x.event_cycle < cycle && !x.is_translated && x.translate_issued;
  };
  auto q_it = std::stable_partition(std::begin(inflight_tag_check), std::end(inflight_tag_check), std::not_fn(missed));

  // Move them to the stash
  translation_stash.insert(std::cend(translation_stash), q_it, std::end(inflight_tag_check));
  inflight_tag_check.erase(q_it, std::end(inflight_tag_check));
}

void CACHE::impl_prefetcher_initialize() { pref_module_pimpl->impl_prefetcher_initialize(); }

uint32_t CACHE::impl_prefetcher_cache_operate(champsim::address addr, champsim::address ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)
{
  return pref_module_pimpl->impl_prefetcher_cache_operate(addr, ip, cache_hit, type, metadata_in);
}

uint32_t CACHE::impl_prefetcher_cache_fill(champsim::address addr, long set, long way, uint8_t prefetch, champsim::address evicted_addr, uint32_t metadata_in)
{
  return pref_module_pimpl->impl_prefetcher_cache_fill(addr, set, way, prefetch, evicted_addr, metadata_in);
}

void CACHE::impl_prefetcher_cycle_operate() { pref_module_pimpl->impl_prefetcher_cycle_operate(); }

void CACHE::impl_prefetcher_final_stats() { pref_module_pimpl->impl_prefetcher_final_stats(); }

void CACHE::impl_prefetcher_branch_operate(champsim::address ip, uint8_t branch_type, champsim::address branch_target)
{
  pref_module_pimpl->impl_prefetcher_branch_operate(ip, branch_type, branch_target);
}

void CACHE::impl_initialize_replacement() { repl_module_pimpl->impl_initialize_replacement(); }

long CACHE::impl_find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set, const BLOCK* current_set, champsim::address ip, champsim::address full_addr,
                                 uint32_t type)
{
  return repl_module_pimpl->impl_find_victim(triggering_cpu, instr_id, set, current_set, ip, full_addr, type);
}

void CACHE::impl_update_replacement_state(uint32_t triggering_cpu, long set, long way, champsim::address full_addr, champsim::address ip, champsim::address victim_addr,
                                          uint32_t type, uint8_t hit)
{
  repl_module_pimpl->impl_update_replacement_state(triggering_cpu, set, way, full_addr, ip, victim_addr, type, hit);
}

void CACHE::impl_replacement_final_stats() { repl_module_pimpl->impl_replacement_final_stats(); }

std::size_t CACHE::get_occupancy(uint8_t queue_type, uint64_t addr) const
{
  return get_occupancy(queue_type, champsim::address{addr});
}

std::size_t CACHE::get_occupancy(uint8_t queue_type, champsim::address) const
{
  if (queue_type == 0)
    return std::size(MSHR);
  // else if (queue_type == 1)
  // return std::size(upper_levels->RQ);
  // else if (queue_type == 2)
  // return std::size(upper_levels->WQ);
  // else if (queue_type == 3)
  // return std::size(upper_levels->PQ);

  return 0;
}

std::size_t CACHE::get_size(uint8_t queue_type, champsim::address) const
{
  if (queue_type == 0)
    return MSHR_SIZE;
  // else if (queue_type == 1)
  // return upper_levels->RQ_SIZE;
  // else if (queue_type == 2)
  // return upper_levels->WQ_SIZE;
  // else if (queue_type == 3)
  // return upper_levels->PQ_SIZE;

  return 0;
}

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
  for (auto type : {LOAD, RFO, PREFETCH, WRITE, TRANSLATION}) {
    roi_stats.hits.at(type).at(finished_cpu) = sim_stats.hits.at(type).at(finished_cpu);
    roi_stats.misses.at(type).at(finished_cpu) = sim_stats.misses.at(type).at(finished_cpu);
  }

  roi_stats.pf_requested = sim_stats.pf_requested;
  roi_stats.pf_issued = sim_stats.pf_issued;
  roi_stats.pf_useful = sim_stats.pf_useful;
  roi_stats.pf_useless = sim_stats.pf_useless;
  roi_stats.pf_fill = sim_stats.pf_fill;

  roi_stats.total_miss_latency = sim_stats.total_miss_latency;

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
  return ((1 << pkt.type) & pref_activate_mask) && !pkt.prefetch_from_this;
}

void CACHE::print_deadlock()
{
  if (!std::empty(MSHR)) {
    std::cout << NAME << " MSHR Entry" << std::endl;
    std::size_t j = 0;
    for (mshr_type entry : MSHR) {
      std::cout << "[" << NAME << " MSHR] entry: " << j++ << " instr_id: " << entry.instr_id;
      std::cout << " address: " << entry.address << " v_addr: " << entry.v_address << " type: " << access_type_names.at(entry.type);
      std::cout << " event_cycle: " << entry.event_cycle << std::endl;
    }
  } else {
    std::cout << NAME << " MSHR empty" << std::endl;
  }

  for (auto ul : upper_levels) {
    if (!std::empty(ul->RQ)) {
      for (const auto& entry : ul->RQ) {
        std::cout << "[" << NAME << " RQ] " << " instr_id: " << entry.instr_id;
        std::cout << " address: " << entry.address << " v_addr: " << entry.v_address << " type: " << access_type_names.at(entry.type) << std::endl;
      }
    } else {
      std::cout << NAME << " RQ empty" << std::endl;
    }

    if (!std::empty(ul->WQ)) {
      for (const auto& entry : ul->WQ) {
        std::cout << "[" << NAME << " WQ] " << " instr_id: " << entry.instr_id;
        std::cout << " address: " << entry.address << " v_addr: " << entry.v_address << " type: " << access_type_names.at(entry.type) << std::endl;
      }
    } else {
      std::cout << NAME << " WQ empty" << std::endl;
    }

    if (!std::empty(ul->PQ)) {
      for (const auto& entry : ul->PQ) {
        std::cout << "[" << NAME << " PQ] " << " instr_id: " << entry.instr_id;
        std::cout << " address: " << entry.address << " v_addr: " << entry.v_address << " type: " << access_type_names.at(entry.type) << std::endl;
      }
    } else {
      std::cout << NAME << " PQ empty" << std::endl;
    }
  }
}
