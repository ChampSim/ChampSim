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

#include "ptw.h"

#include <algorithm> // for for_each, partition, partition_copy
#include <array>     // for array, get, swap
#include <cmath>
#include <iterator> // for back_insert_iterator, begin, end, size
#include <memory>   // for allocator_traits<>::value_type
#include <numeric>
#include <string_view> // for string_view
#include <tuple>       // for tie, tuple
#include <utility>     // for tuple_element<>::type, pair
#include <fmt/core.h>

#include "champsim.h"
#include "champsim_constants.h"
#include "ptw_builder.h" // for ptw_builder
#include "util/bits.h"   // for bitmask, lg2, splice_bits
#include "util/span.h"
#include "vmem.h"

PageTableWalker::PageTableWalker(champsim::ptw_builder b)
    : champsim::operable(b.m_freq_scale), upper_levels(b.m_uls), lower_level(b.m_ll), NAME(b.m_name),
      MSHR_SIZE(b.m_mshr_size.value_or(std::lround(b.m_mshr_factor * std::floor(std::size(upper_levels))))),
      MAX_READ(b.m_max_tag_check.value_or(b.m_bandwidth_factor * std::floor(std::size(upper_levels)))),
      MAX_FILL(b.m_max_fill.value_or(b.m_bandwidth_factor * std::floor(std::size(upper_levels)))), HIT_LATENCY(b.m_latency), vmem(b.m_vmem),
      CR3_addr(b.m_vmem->get_pte_pa(b.m_cpu, champsim::address{}, b.m_vmem->pt_levels).first)
{
  std::vector<std::array<uint32_t, 3>> local_pscl_dims{};
  std::remove_copy_if(std::begin(b.m_pscl), std::end(b.m_pscl), std::back_inserter(local_pscl_dims), [](auto x) { return std::get<0>(x) == 0; });
  std::sort(std::begin(local_pscl_dims), std::end(local_pscl_dims), std::greater{});

  for (auto [level, sets, ways] : local_pscl_dims) {
    pscl.emplace_back(sets, ways, pscl_indexer{b.m_vmem->shamt(level)}, pscl_indexer{b.m_vmem->shamt(level)});
  }
}

PageTableWalker::mshr_type::mshr_type(const request_type& req, std::size_t level)
    : address(req.address), v_address(req.v_address), instr_depend_on_me(req.instr_depend_on_me), pf_metadata(req.pf_metadata), cpu(req.cpu),
      translation_level(level)
{
  asid[0] = req.asid[0];
  asid[1] = req.asid[1];
}

auto PageTableWalker::handle_read(const request_type& handle_pkt, channel_type* ul) -> std::optional<mshr_type>
{
  pscl_entry walk_init = {handle_pkt.v_address, CR3_addr, std::size(pscl)};
  std::vector<std::optional<pscl_entry>> pscl_hits;
  std::transform(std::begin(pscl), std::end(pscl), std::back_inserter(pscl_hits), [walk_init](auto& x) { return x.check_hit(walk_init); });
  walk_init =
      std::accumulate(std::begin(pscl_hits), std::end(pscl_hits), std::optional<pscl_entry>(walk_init), [](auto x, auto& y) { return y.value_or(*x); }).value();

  champsim::address_slice<LOG2_PAGE_SIZE, champsim::lg2(PTE_BYTES)> walk_offset{vmem->get_offset(handle_pkt.address, walk_init.level)};

  mshr_type fwd_mshr{handle_pkt, walk_init.level};
  fwd_mshr.address = champsim::splice(champsim::page_number{walk_init.ptw_addr}, champsim::page_offset{walk_offset});
  fwd_mshr.v_address = handle_pkt.address;
  if (handle_pkt.response_requested) {
    fwd_mshr.to_return = {&ul->returned};
  }

  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} address: {} v_address: {} pt_page_offset: {} translation_level: {}\n", NAME, __func__, walk_init.vaddr, handle_pkt.v_address,
               walk_offset.to<int>(), walk_init.level);
  }

  return step_translation(fwd_mshr);
}

auto PageTableWalker::handle_fill(const mshr_type& fill_mshr) -> std::optional<mshr_type>
{
  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} address: {} v_address: {} data: {} pt_page_offset: {} translation_level: {} event: {} current: {}\n", NAME, __func__,
               fill_mshr.address, fill_mshr.v_address, fill_mshr.data, fill_mshr.data.slice<LOG2_PAGE_SIZE+champsim::lg2(PTE_BYTES), LOG2_PAGE_SIZE>().to<int>(),
               fill_mshr.translation_level, fill_mshr.event_cycle, current_cycle);
  }

  const auto pscl_idx = std::size(pscl) - fill_mshr.translation_level;
  pscl.at(pscl_idx).fill({fill_mshr.v_address, fill_mshr.data, fill_mshr.translation_level - 1});

  mshr_type fwd_mshr = fill_mshr;
  fwd_mshr.address = fill_mshr.data;
  fwd_mshr.translation_level = fill_mshr.translation_level - 1;
  fwd_mshr.event_cycle = std::numeric_limits<uint64_t>::max();

  return step_translation(fwd_mshr);
}

auto PageTableWalker::step_translation(const mshr_type& source) -> std::optional<mshr_type>
{
  request_type packet;
  packet.address = source.address;
  packet.v_address = source.v_address;
  packet.pf_metadata = source.pf_metadata;
  packet.cpu = source.cpu;
  packet.asid[0] = source.asid[0];
  packet.asid[1] = source.asid[1];
  packet.is_translated = true;
  packet.type = access_type::TRANSLATION;

  bool success = lower_level->add_rq(packet);
  if (success) {
    return source;
  }

  return std::nullopt;
}

void PageTableWalker::operate()
{
  for (const auto& x : lower_level->returned) {
    finish_packet(x);
  }
  lower_level->returned.clear();

  std::vector<mshr_type> next_steps{};

  auto fill_bw = MAX_FILL;
  auto [complete_begin, complete_end] = champsim::get_span_p(std::cbegin(completed), std::cend(completed), fill_bw,
                                                             [cycle = current_cycle](const auto& pkt) { return pkt.event_cycle <= cycle; });
  std::for_each(complete_begin, complete_end, [](auto& mshr_entry) {
    for (auto ret : mshr_entry.to_return) {
      ret->emplace_back(mshr_entry.v_address, mshr_entry.v_address, mshr_entry.data, mshr_entry.pf_metadata, mshr_entry.instr_depend_on_me);
    }
  });
  fill_bw -= std::distance(complete_begin, complete_end);
  completed.erase(complete_begin, complete_end);

  auto [mshr_begin, mshr_end] =
      champsim::get_span_p(std::cbegin(finished), std::cend(finished), fill_bw, [cycle = current_cycle](const auto& pkt) { return pkt.event_cycle <= cycle; });
  std::tie(mshr_begin, mshr_end) = champsim::get_span_p(mshr_begin, mshr_end, [&next_steps, this](const auto& pkt) {
    auto result = this->handle_fill(pkt);
    if (result.has_value()) {
      next_steps.push_back(*result);
    }
    return result.has_value();
  });
  finished.erase(mshr_begin, mshr_end);

  auto tag_bw = MAX_READ;
  for (auto* ul : upper_levels) {
    auto [rq_begin, rq_end] = champsim::get_span_p(std::cbegin(ul->RQ), std::cend(ul->RQ), tag_bw, [&next_steps, ul, this](const auto& pkt) {
      auto result = this->handle_read(pkt, ul);
      if (result.has_value()) {
        next_steps.push_back(*result);
      }
      return result.has_value();
    });
    tag_bw -= std::distance(rq_begin, rq_end);
    ul->RQ.erase(rq_begin, rq_end);
  }

  MSHR.insert(std::cend(MSHR), std::begin(next_steps), std::end(next_steps));
}

void PageTableWalker::finish_packet(const response_type& packet)
{
  auto finish_step = [this](auto& mshr_entry) {
    uint64_t penalty = 0;
    std::tie(mshr_entry.data, penalty) = this->vmem->get_pte_pa(mshr_entry.cpu, mshr_entry.v_address, mshr_entry.translation_level);
    mshr_entry.event_cycle = this->current_cycle + (this->warmup ? 0 : penalty + HIT_LATENCY);

    if constexpr (champsim::debug_print) {
      fmt::print("[{}] finish_packet address: {} v_address: {} data: {} translation_level: {}\n", NAME, mshr_entry.address, mshr_entry.v_address,
                 mshr_entry.data, mshr_entry.translation_level);
    }
  };

  auto finish_last_step = [this](auto& mshr_entry) {
    uint64_t penalty = 0;
    std::tie(mshr_entry.data, penalty) = this->vmem->va_to_pa(mshr_entry.cpu, mshr_entry.v_address);
    mshr_entry.event_cycle = this->current_cycle + (this->warmup ? 0 : penalty + HIT_LATENCY);

    if constexpr (champsim::debug_print) {
      fmt::print("[{}] complete_packet address: {} v_address: {} data: {} translation_level: {}\n", this->NAME, mshr_entry.address,
                 mshr_entry.v_address, mshr_entry.data, mshr_entry.translation_level);
    }
  };

  auto last_finished =
      std::partition(std::begin(MSHR), std::end(MSHR), [match = champsim::block_number{packet.address}](auto x) { return champsim::block_number{x.address} == match; });

  std::for_each(std::begin(MSHR), last_finished, [finish_step, finish_last_step](auto& mshr_entry) {
    if (mshr_entry.translation_level > 0) {
      finish_step(mshr_entry);
    } else {
      finish_last_step(mshr_entry);
    }
  });

  std::partition_copy(std::begin(MSHR), last_finished, std::back_inserter(finished), std::back_inserter(completed),
                      [](auto x) { return x.translation_level > 0; });
  MSHR.erase(std::begin(MSHR), last_finished);
}

void PageTableWalker::begin_phase()
{
  for (auto* ul : upper_levels) {
    channel_type::stats_type ul_new_roi_stats;
    channel_type::stats_type ul_new_sim_stats;
    ul->roi_stats = ul_new_roi_stats;
    ul->sim_stats = ul_new_sim_stats;
  }
}

// LCOV_EXCL_START Exclude the following function from LCOV
void PageTableWalker::print_deadlock()
{
  if (!std::empty(MSHR)) {
    fmt::print("{} MSHR Entry\n", NAME);
    std::size_t j = 0;
    for (auto entry : MSHR) {
      fmt::print("[{}_MSHR] {} address: {} v_address: {} translation_level: {} event_cycle: {}\n", NAME, j++, entry.address, entry.v_address,
                 entry.translation_level, entry.event_cycle);
    }
  } else {
    fmt::print("{} MSHR empty\n", NAME);
  }
}
// LCOV_EXCL_STOP
