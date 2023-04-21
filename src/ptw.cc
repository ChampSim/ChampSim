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

#include <numeric>

#include <fmt/core.h>

#include "champsim.h"
#include "champsim_constants.h"
#include "instruction.h"
#include "util.h"
#include "vmem.h"

PageTableWalker::PageTableWalker(Builder b)
    : champsim::operable(b.m_freq_scale), upper_levels(b.m_uls), lower_level(b.m_ll), NAME(b.m_name), MSHR_SIZE(b.m_mshr_size), MAX_READ(b.m_max_tag_check),
      MAX_FILL(b.m_max_fill), HIT_LATENCY(b.m_latency), vmem(b.m_vmem), CR3_addr(b.m_vmem->get_pte_pa(b.m_cpu, 0, b.m_vmem->pt_levels).first)
{
  std::vector<std::array<uint32_t, 3>> local_pscl_dims{};
  std::remove_copy_if(std::begin(b.m_pscl), std::end(b.m_pscl), std::back_inserter(local_pscl_dims), [](auto x) { return std::get<0>(x) == 0; });
  std::sort(std::begin(local_pscl_dims), std::end(local_pscl_dims), std::greater{});

  for (auto [level, sets, ways] : local_pscl_dims)
    pscl.emplace_back(sets, ways, pscl_indexer{b.m_vmem->shamt(level)}, pscl_indexer{b.m_vmem->shamt(level)});
}

PageTableWalker::mshr_type::mshr_type(request_type req, std::size_t level)
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

  auto walk_offset = vmem->get_offset(handle_pkt.address, walk_init.level) * PTE_BYTES;

  mshr_type fwd_mshr{handle_pkt, walk_init.level};
  fwd_mshr.address = champsim::splice_bits(walk_init.ptw_addr, walk_offset, LOG2_PAGE_SIZE);
  fwd_mshr.v_address = handle_pkt.address;
  if (handle_pkt.response_requested)
    fwd_mshr.to_return = {&ul->returned};

  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} address: {:x} v_address: {:x} pt_page_offset: {} translation_level: {}\n",
        NAME, __func__, walk_init.vaddr, handle_pkt.v_address, walk_offset / PTE_BYTES, walk_init.level);
  }

  return step_translation(fwd_mshr);
}

auto PageTableWalker::handle_fill(const mshr_type& fill_mshr) -> std::optional<mshr_type>
{
  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} address: {:x} v_address: {:x} data: {:x} pt_page_offset: {} translation_level: {} event: {} current: {}\n",
        NAME, __func__, fill_mshr.address, fill_mshr.v_address, fill_mshr.data,
        (fill_mshr.data & champsim::bitmask(LOG2_PAGE_SIZE)) >> champsim::lg2(PTE_BYTES), fill_mshr.translation_level, fill_mshr.event_cycle, current_cycle);
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
  packet.type = TRANSLATION;

  bool success = lower_level->add_rq(packet);

  if (success)
    return source;

  return std::nullopt;
}

void PageTableWalker::operate()
{
  for (const auto& x : lower_level->returned)
    finish_packet(x);
  lower_level->returned.clear();

  std::vector<mshr_type> next_steps{};

  auto fill_bw = MAX_FILL;
  auto [complete_begin, complete_end] = champsim::get_span_p(std::cbegin(completed), std::cend(completed), fill_bw,
                                                             [cycle = current_cycle](const auto& pkt) { return pkt.event_cycle <= cycle; });
  std::for_each(complete_begin, complete_end, [](auto& mshr_entry) {
    for (auto ret : mshr_entry.to_return)
      ret->emplace_back(mshr_entry.v_address, mshr_entry.v_address, mshr_entry.data, mshr_entry.pf_metadata, mshr_entry.instr_depend_on_me);
  });
  fill_bw -= std::distance(complete_begin, complete_end);
  completed.erase(complete_begin, complete_end);

  auto [mshr_begin, mshr_end] =
      champsim::get_span_p(std::cbegin(finished), std::cend(finished), fill_bw, [cycle = current_cycle](const auto& pkt) { return pkt.event_cycle <= cycle; });
  std::tie(mshr_begin, mshr_end) = champsim::get_span_p(mshr_begin, mshr_end, [&next_steps, this](const auto& pkt) {
    auto result = this->handle_fill(pkt);
    if (result.has_value())
      next_steps.push_back(*result);
    return result.has_value();
  });
  finished.erase(mshr_begin, mshr_end);

  auto tag_bw = MAX_READ;
  for (auto ul : upper_levels) {
    auto [rq_begin, rq_end] = champsim::get_span_p(std::cbegin(ul->RQ), std::cend(ul->RQ), tag_bw, [&next_steps, ul, this](const auto& pkt) {
      auto result = this->handle_read(pkt, ul);
      if (result.has_value())
        next_steps.push_back(*result);
      return result.has_value();
    });
    tag_bw -= std::distance(rq_begin, rq_end);
    ul->RQ.erase(rq_begin, rq_end);
  }

  MSHR.insert(std::cend(MSHR), std::begin(next_steps), std::end(next_steps));
}

void PageTableWalker::finish_packet(const response_type& packet)
{
  auto last_finished =
      std::partition(std::begin(MSHR), std::end(MSHR), [addr = packet.address](auto x) { return (x.address >> LOG2_BLOCK_SIZE) == (addr >> LOG2_BLOCK_SIZE); });
  auto inserted_finished = finished.insert(std::cend(finished), std::begin(MSHR), last_finished);
  MSHR.erase(std::begin(MSHR), last_finished);

  auto last_unfinished = std::partition(inserted_finished, std::end(finished), [](auto x) { return x.translation_level > 0; });
  std::for_each(inserted_finished, last_unfinished, [this](auto& mshr_entry) {
    uint64_t penalty;
    std::tie(mshr_entry.data, penalty) = this->vmem->get_pte_pa(mshr_entry.cpu, mshr_entry.v_address, mshr_entry.translation_level);
    mshr_entry.event_cycle = this->current_cycle + (this->warmup ? 0 : penalty + HIT_LATENCY);

    if constexpr (champsim::debug_print) {
      fmt::print("[{}] {} address: {:x} v_address: {:x} data: {:x} translation_level: {}\n",
          NAME, __func__, mshr_entry.address, mshr_entry.v_address, mshr_entry.data, mshr_entry.translation_level);
    }
  });

  std::for_each(last_unfinished, std::end(finished), [this](auto& mshr_entry) {
    uint64_t penalty;
    std::tie(mshr_entry.data, penalty) = this->vmem->va_to_pa(mshr_entry.cpu, mshr_entry.v_address);
    mshr_entry.event_cycle = this->current_cycle + (this->warmup ? 0 : penalty + HIT_LATENCY);

    if constexpr (champsim::debug_print) {
      fmt::print("[{}] complete_packet address: {:x} v_address: {:x} data: {:x} translation_level: {}\n",
          this->NAME, mshr_entry.address, mshr_entry.v_address, mshr_entry.data, +mshr_entry.translation_level);
    }
  });

  completed.insert(std::cend(completed), last_unfinished, std::end(finished));
  finished.erase(last_unfinished, std::end(finished));
}

void PageTableWalker::begin_phase()
{
  for (auto ul : upper_levels) {
    channel_type::stats_type ul_new_roi_stats, ul_new_sim_stats;
    ul->roi_stats = ul_new_roi_stats;
    ul->sim_stats = ul_new_sim_stats;
  }
}

void PageTableWalker::print_deadlock()
{
  if (!std::empty(MSHR)) {
    fmt::print("{} MSHR Entry\n", NAME);
    std::size_t j = 0;
    for (auto entry : MSHR) {
      fmt::print("[{}_MSHR] {} address: {:x} v_address: {:x} translation_level: {} event_cycle: {}\n",
          NAME, j++, entry.address, entry.v_address, +entry.translation_level, entry.event_cycle);
    }
  } else {
    fmt::print("{} MSHR empty\n", NAME);
  }
}
