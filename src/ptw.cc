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

#include <cmath>
#include <numeric>
#include <fmt/chrono.h>
#include <fmt/core.h>

#include "champsim.h"
#include "deadlock.h"
#include "instruction.h"
#include "util/bits.h" // for bitmask, lg2, splice_bits
#include "util/span.h"
#include "vmem.h"

PageTableWalker::PageTableWalker(champsim::modules::ModuleBuilder builder)
    : champsim::modules::page_table_walker_module(builder.get_parameter<champsim::chrono::picoseconds>("clock_period")),
      upper_levels(builder.get_parameter<std::vector<champsim::modules::channel_module*>>("upper_levels")),
      lower_level(builder.get_parameter<champsim::modules::channel_module*>("lower_level")), NAME(builder.get_name()),
      MSHR_SIZE(builder.get_parameter<uint32_t>("mshr_size")), MAX_READ(builder.get_parameter<champsim::bandwidth::maximum_type>("max_tag_check")),
      MAX_FILL(builder.get_parameter<champsim::bandwidth::maximum_type>("max_fill")),
      HIT_LATENCY(builder.get_parameter<unsigned>("latency") * builder.get_parameter<champsim::chrono::picoseconds>("clock_period")),
      vmem(builder.get_parameter<champsim::modules::vmem_module*>("vmem")), pt_levels_(vmem->get_pt_levels())
{
  log2_page_size_ = builder.get_parameter<unsigned>("log2_page_size");
  auto local_pscl_dims = builder.get_parameter<std::vector<std::array<uint32_t, 3>>>("pscl_dims");
  auto pt_levels = vmem->get_pt_levels();
  // Valid PSCL levels are [2, pt_levels]. Level 1 is never cached (handle_fill
  // at level 1 produces the final va_to_pa lookup, not another intermediate
  // step). Levels > pt_levels don't exist in this table.
  local_pscl_dims.erase(
      std::remove_if(std::begin(local_pscl_dims), std::end(local_pscl_dims), [pt_levels](auto x) { return std::get<0>(x) > pt_levels || std::get<0>(x) < 2; }),
      std::end(local_pscl_dims));

  // Ensure every level in [2, pt_levels] has a PSCL entry. Missing levels get
  // a 0-way stub that always misses and silently ignores fills, preserving the
  // invariant std::size(pscl) == pt_levels - 1 so the walk always starts at
  // the correct level and no walk steps are silently skipped.
  for (std::size_t level = 2; level <= pt_levels; ++level) {
    bool configured = std::any_of(std::begin(local_pscl_dims), std::end(local_pscl_dims), [level](const auto& x) { return std::get<0>(x) == level; });
    if (!configured) {
      local_pscl_dims.push_back({static_cast<uint32_t>(level), 1, 0});
    }
  }

  std::sort(std::begin(local_pscl_dims), std::end(local_pscl_dims), std::greater{});

  for (auto [level, sets, ways] : local_pscl_dims) {
    pscl.emplace_back(sets, ways, pscl_indexer{vmem->shamt(level)}, pscl_tagger{vmem->shamt(level)});
  }
}

PageTableWalker::mshr_type::mshr_type(const request_type& req, std::size_t level)
    : address(req.address), v_address(req.v_address), instr_depend_on_me(req.instr_depend_on_me), pf_metadata(req.pf_metadata), origin(req.origin),
      translation_level(level)
{
}

auto PageTableWalker::handle_read(const request_type& handle_pkt, channel_type* ul) -> std::optional<mshr_type>
{
  // The walk's address space comes from the request, not from the walker:
  // this is hardware owned by a consumer, serving whatever address spaces reach it.
  // The root (CR3) is the requesting address space's, resolved per walk.
  const auto walk_root = vmem->get_pte_pa(handle_pkt.origin, champsim::page_number{}, pt_levels_).first;
  pscl_entry walk_init = {handle_pkt.v_address, walk_root, std::size(pscl), handle_pkt.origin.asid()};
  std::vector<std::optional<pscl_entry>> pscl_hits;
  std::transform(std::begin(pscl), std::end(pscl), std::back_inserter(pscl_hits), [walk_init](auto& x) { return x.check_hit(walk_init); });
  walk_init =
      std::accumulate(std::begin(pscl_hits), std::end(pscl_hits), std::optional<pscl_entry>(walk_init), [](auto x, auto& y) { return y.value_or(*x); }).value();

  champsim::address_slice walk_offset{
      champsim::dynamic_extent{champsim::data::bits{log2_page_size_}, champsim::data::bits{champsim::lg2(pte_entry::byte_multiple)}},
      vmem->get_offset(handle_pkt.address, walk_init.level)};

  mshr_type fwd_mshr{handle_pkt, walk_init.level};
  fwd_mshr.address = champsim::address{champsim::splice(champsim::page_number{walk_init.ptw_addr}, champsim::page_offset{walk_offset})};
  fwd_mshr.v_address = handle_pkt.address;
  if (handle_pkt.response_requested) {
    fwd_mshr.to_return = {&ul->get_returned()};
  }

  if constexpr (champsim::debug_print) {
    fmt::print("[{}] {} address: {} v_address: {} pt_page_offset: {} translation_level: {} cycle: {}\n", NAME, __func__, fwd_mshr.address, handle_pkt.v_address,
               walk_offset.to<int>(), walk_init.level, current_time.time_since_epoch() / clock_period);
  }

  return step_translation(fwd_mshr);
}

auto PageTableWalker::handle_fill(const mshr_type& fill_mshr) -> std::optional<mshr_type>
{
  if constexpr (champsim::debug_print) {
    champsim::dynamic_extent pte_offset_extent{champsim::data::bits{log2_page_size_}, champsim::data::bits{champsim::lg2(pte_entry::byte_multiple)}};
    fmt::print("[{}] {} address: {} v_address: {} data: {} pt_page_offset: {} translation_level: {} cycle: {}\n", NAME, __func__, fill_mshr.address,
               fill_mshr.v_address, *fill_mshr.data, champsim::address_slice{pte_offset_extent, fill_mshr.data.value()}.to<int>(), fill_mshr.translation_level,
               current_time.time_since_epoch() / clock_period);
  }

  const auto pscl_idx = std::size(pscl) - fill_mshr.translation_level;
  pscl.at(pscl_idx).fill({fill_mshr.v_address, *fill_mshr.data, fill_mshr.translation_level, fill_mshr.origin.asid()});

  mshr_type fwd_mshr = fill_mshr;
  fwd_mshr.address = *fill_mshr.data;
  fwd_mshr.translation_level = fill_mshr.translation_level - 1;

  return step_translation(fwd_mshr);
}

auto PageTableWalker::step_translation(const mshr_type& source) -> std::optional<mshr_type>
{
  request_type packet;
  packet.address = source.address;
  packet.v_address = source.v_address;
  packet.pf_metadata = source.pf_metadata;
  packet.origin = source.origin;
  packet.is_translated = true;
  packet.type = access_type::TRANSLATION;

  bool success = lower_level->add_rq(packet);
  if (success) {
    return source;
  }

  return std::nullopt;
}

long PageTableWalker::poll_cycle()
{
  // Skip only when no walk state exists anywhere: nothing returned from
  // below, no in-flight walk steps, and no requests on any upper channel.
  // MSHR entries awaiting a lower-level response are skippable — the wake
  // event is an arrival on lower_level->get_returned(), re-checked here
  // every cycle.
  const bool idle = std::empty(lower_level->get_returned()) && std::empty(finished) && std::empty(completed)
                    && std::all_of(std::cbegin(upper_levels), std::cend(upper_levels), [](auto* ul) { return std::empty(ul->get_rq()); });
  return idle ? 1 : 0;
}

long PageTableWalker::operate()
{
  long progress{0};

  auto is_ready = [time = current_time](const auto& pkt) {
    return pkt.data.is_ready_at(time);
  };
  std::for_each(std::cbegin(lower_level->get_returned()), std::cend(lower_level->get_returned()), [this](const auto& pkt) { this->finish_packet(pkt); });
  progress += std::distance(std::cbegin(lower_level->get_returned()), std::cend(lower_level->get_returned()));
  lower_level->get_returned().clear();

  std::vector<mshr_type> next_steps{};

  champsim::bandwidth fill_bw{MAX_FILL};
  auto [complete_begin, complete_end] = champsim::get_span_p(std::cbegin(completed), std::cend(completed), fill_bw, is_ready);
  std::for_each(complete_begin, complete_end, [](auto& mshr_entry) {
    for (auto ret : mshr_entry.to_return) {
      ret->emplace_back(mshr_entry.v_address, mshr_entry.v_address, *mshr_entry.data, mshr_entry.pf_metadata, mshr_entry.instr_depend_on_me);
    }
  });
  fill_bw.consume(std::distance(complete_begin, complete_end));
  completed.erase(complete_begin, complete_end);

  auto [mshr_begin, mshr_end] = champsim::get_span_p(std::cbegin(finished), std::cend(finished), fill_bw, is_ready);
  std::tie(mshr_begin, mshr_end) = champsim::get_span_p(mshr_begin, mshr_end, [&next_steps, this](const auto& pkt) {
    auto result = this->handle_fill(pkt);
    if (result.has_value()) {
      next_steps.emplace_back(*result);
    }
    return result.has_value();
  });
  fill_bw.consume(std::distance(mshr_begin, mshr_end));
  finished.erase(mshr_begin, mshr_end);

  champsim::bandwidth tag_bw{MAX_READ};
  for (auto* ul : upper_levels) {
    auto [rq_begin, rq_end] = champsim::get_span_p(std::cbegin(ul->get_rq()), std::cend(ul->get_rq()), tag_bw, [&next_steps, ul, this](const auto& pkt) {
      auto result = this->handle_read(pkt, ul);
      if (result.has_value()) {
        next_steps.emplace_back(*result);
      }
      return result.has_value();
    });
    tag_bw.consume(std::distance(rq_begin, rq_end));
    ul->get_rq().erase(rq_begin, rq_end);
  }

  MSHR.insert(std::cend(MSHR), std::begin(next_steps), std::end(next_steps));
  progress += fill_bw.amount_consumed() + tag_bw.amount_consumed();

  if constexpr (champsim::debug_print) {
    if (progress > 0) {
      std::vector<champsim::address> mshr_addresses{};
      std::transform(std::begin(MSHR), std::end(MSHR), std::back_inserter(mshr_addresses), [](const auto& x) { return x.address; });
      fmt::print("[{}] {} MSHR contents: {} cycle: {}\n", NAME, __func__, mshr_addresses, current_time.time_since_epoch() / clock_period);
    }
  }

  return progress;
}

void PageTableWalker::finish_packet(const response_type& packet)
{
  auto finish_step = [this](auto mshr_entry) {
    auto [ppage, penalty] = this->vmem->get_pte_pa(mshr_entry.origin, champsim::page_number{mshr_entry.v_address}, mshr_entry.translation_level);

    if constexpr (champsim::debug_print) {
      fmt::print("[{}] finish_packet address: {} v_address: {} data: {} translation_level: {} cycle: {} penalty: {}\n", NAME, mshr_entry.address,
                 mshr_entry.v_address, ppage, mshr_entry.translation_level, this->current_time.time_since_epoch() / this->clock_period,
                 penalty / this->clock_period);
    }

    return champsim::waitable{ppage, this->current_time + penalty + (this->is_warmup() ? champsim::chrono::clock::duration{} : HIT_LATENCY)};
  };

  auto finish_last_step = [this](auto mshr_entry) {
    auto [ppage, penalty] = this->vmem->va_to_pa(mshr_entry.origin, champsim::page_number{mshr_entry.v_address});

    if constexpr (champsim::debug_print) {
      fmt::print("[{}] complete_packet address: {} v_address: {} data: {} translation_level: {} clock: {} penalty: {}\n", NAME, mshr_entry.address,
                 mshr_entry.v_address, ppage, mshr_entry.translation_level, this->current_time.time_since_epoch() / this->clock_period,
                 penalty / this->clock_period);
    }

    return champsim::waitable{champsim::address{ppage}, this->current_time + penalty + (this->is_warmup() ? champsim::chrono::clock::duration{} : HIT_LATENCY)};
  };

  auto matches_addr = [block = champsim::block_number{packet.address}](auto x) {
    return champsim::block_number{x.address} == block;
  };
  auto is_last_step = [](auto x) {
    return x.translation_level <= 0;
  };
  auto last_finished = std::partition(std::begin(MSHR), std::end(MSHR), matches_addr);

  std::for_each(std::begin(MSHR), last_finished, [is_last_step, finish_step, finish_last_step](auto& mshr_entry) {
    mshr_entry.data = is_last_step(mshr_entry) ? finish_last_step(mshr_entry) : finish_step(mshr_entry);
  });

  std::partition_copy(std::begin(MSHR), last_finished, std::back_inserter(completed), std::back_inserter(finished), is_last_step);
  MSHR.erase(std::begin(MSHR), last_finished);
}

void PageTableWalker::begin_phase(bool warmup)
{
  warmup_ = warmup;
  for (auto* ul : upper_levels) {
    channel_type::stats_type ul_new_sim_stats;
    ul->get_sim_stats() = ul_new_sim_stats;
  }
}

// LCOV_EXCL_START Exclude the following function from LCOV
void PageTableWalker::print_deadlock()
{
  champsim::range_print_deadlock(MSHR, NAME + "_MSHR", "address: {} v_address: {} translation_level: {}", [](const auto& entry) {
    return std::tuple{entry.address, entry.v_address, entry.translation_level};
  });
}
// LCOV_EXCL_STOP

champsim::modules::page_table_walker_module::register_module<PageTableWalker> ptw_module("DEFAULT_PTW");