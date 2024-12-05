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

#ifndef CORE_BUILDER_H
#define CORE_BUILDER_H

#include <cstdint>
#include <limits>
#include <vector>
#include <string>

#include "chrono.h"
#include "bandwidth.h"

class CACHE;
class O3_CPU;
namespace champsim
{
class channel;

namespace detail
{
struct core_builder_base {
  uint32_t m_cpu{};
  champsim::chrono::picoseconds m_clock_period{250};
  std::size_t m_dib_set{1};
  std::size_t m_dib_way{1};
  std::size_t m_dib_window{1};
  std::size_t m_ifetch_buffer_size{1};
  std::size_t m_decode_buffer_size{1};
  std::size_t m_dispatch_buffer_size{1};

  std::size_t m_dib_hit_buffer_size{1};

  std::size_t m_register_file_size{1};
  std::size_t m_rob_size{1};
  std::size_t m_lq_size{1};
  std::size_t m_sq_size{1};

  champsim::bandwidth::maximum_type m_fetch_width{1};
  champsim::bandwidth::maximum_type m_decode_width{1};
  champsim::bandwidth::maximum_type m_dispatch_width{1};
  champsim::bandwidth::maximum_type m_schedule_width{1};
  champsim::bandwidth::maximum_type m_execute_width{1};
  champsim::bandwidth::maximum_type m_lq_width{1};
  champsim::bandwidth::maximum_type m_sq_width{1};
  champsim::bandwidth::maximum_type m_retire_width{1};
  champsim::bandwidth::maximum_type m_dib_inorder_width{1};

  unsigned m_dib_hit_latency{};

  unsigned m_mispredict_penalty{};
  unsigned m_decode_latency{};
  unsigned m_dispatch_latency{};
  unsigned m_schedule_latency{};
  unsigned m_execute_latency{};

  CACHE* m_l1i{};
  champsim::bandwidth::maximum_type m_l1i_bw{1};
  champsim::bandwidth::maximum_type m_l1d_bw{1};
  champsim::channel* m_fetch_queues{};
  champsim::channel* m_data_queues{};

  std::vector<std::string> m_btb_impls{};
  std::vector<std::string> m_bp_impls{};
};
} // namespace detail

class core_builder : public detail::core_builder_base
{
  using self_type = core_builder;

  friend class ::O3_CPU;

  explicit core_builder(const detail::core_builder_base& other) : detail::core_builder_base(other) {}

public:
  core_builder() = default;

  self_type& index(uint32_t cpu_);

  /**
   * Specify the core's clock period.
   */
  self_type& clock_period(champsim::chrono::picoseconds clock_period_);

  /**
   * Specify the number of sets in the Decoded Instruction Buffer.
   */
  self_type& dib_set(std::size_t dib_set_);

  /**
   * Specify the number of ways in the Decoded Instruction Buffer.
   */
  self_type& dib_way(std::size_t dib_way_);

  /**
   * Specify the size of the window within which Decoded Instruction Buffer entries are equivalent.
   */
  self_type& dib_window(std::size_t dib_window_);

  /**
   * Specify the maximum size of the instruction fetch buffer.
   */
  self_type& ifetch_buffer_size(std::size_t ifetch_buffer_size_);

  /**
   * Specify the maximum size of the decode buffer.
   */
  self_type& decode_buffer_size(std::size_t decode_buffer_size_);

  /**
   * Specify the maximum size of the dispatch buffer.
   */
  self_type& dispatch_buffer_size(std::size_t dispatch_buffer_size_);

  /**
   * Specify the maximum size of the DIB hit buffer.
   */
  self_type& dib_hit_buffer_size(std::size_t dib_hit_buffer_size_);

  /**
   * Specify the maximum size of the physical register file.
   */
  self_type& register_file_size(std::size_t register_file_size_);

  /**
   * Specify the maximum size of the reorder buffer.
   */
  self_type& rob_size(std::size_t rob_size_);

  /**
   * Specify the maximum size of the load queue.
   */
  self_type& lq_size(std::size_t lq_size_);

  /**
   * Specify the maximum size of the store queue.
   */
  self_type& sq_size(std::size_t sq_size_);

  /**
   * Specify the width of the instruction fetch.
   */
  self_type& fetch_width(champsim::bandwidth::maximum_type fetch_width_);

  /**
   * Specify the width of the decode.
   */
  self_type& decode_width(champsim::bandwidth::maximum_type decode_width_);

  /**
   * Specify the width of the dispatch.
   */
  self_type& dispatch_width(champsim::bandwidth::maximum_type dispatch_width_);

  /**
   * Specify the width of the scheduler.
   */
  self_type& schedule_width(champsim::bandwidth::maximum_type schedule_width_);

  /**
   * Specify the width of the execution.
   */
  self_type& execute_width(champsim::bandwidth::maximum_type execute_width_);

  /**
   * Specify the width of the load issue.
   */
  self_type& lq_width(champsim::bandwidth::maximum_type lq_width_);

  /**
   * Specify the width of the store issue.
   */
  self_type& sq_width(champsim::bandwidth::maximum_type sq_width_);

  /**
   * Specify the width of the retirement.
   */
  self_type& retire_width(champsim::bandwidth::maximum_type retire_width_);

  /**
   * Specify the maximum size of the DIB inorder width.
   */
  self_type& dib_inorder_width(champsim::bandwidth::maximum_type dib_inorder_width_);

  /**
   * Specify the reset penalty, in cycles, that follows a misprediction.
   * Note that this value is in addition to the cost of restarting the pipeline, which will depend on the number of instructions inflight at the time when the
   * misprediction is detected.
   */
  self_type& mispredict_penalty(unsigned mispredict_penalty_);

  /**
   * Specify the latency of the decode.
   */
  self_type& decode_latency(unsigned decode_latency_);

  /**
   * Specify the latency of dispatch.
   */
  self_type& dispatch_latency(unsigned dispatch_latency_);

  /**
   * Specify the latency of the scheduler.
   */
  self_type& schedule_latency(unsigned schedule_latency_);

  /**
   * Specify the latency of execution.
   */
  self_type& execute_latency(unsigned execute_latency_);

  /**
   * Specify the latency of execution.
   */
  self_type& dib_hit_latency(unsigned dib_hit_latency_);

  /**
   * Specify a pointer to the L1I cache. This is only used to transmit branch triggers for prefetcher branch hooks.
   */
  self_type& l1i(CACHE* l1i_);

  /**
   * Specify the instruction cache bandwidth.
   */
  self_type& l1i_bandwidth(champsim::bandwidth::maximum_type l1i_bw_);

  /**
   * Specify the data cache bandwidth.
   */
  self_type& l1d_bandwidth(champsim::bandwidth::maximum_type l1d_bw_);

  /**
   * Specify the downstream queues to the instruction cache.
   */
  self_type& fetch_queues(champsim::channel* fetch_queues_);

  /**
   * Specify the downstream queues to the data cache.
   */
  self_type& data_queues(champsim::channel* data_queues_);

  /**
   * Specify the branch direction predictor.
   */
  template <typename... Elems>
  self_type& branch_predictor(Elems... bp_impls);

  /**
   * Specify the branch target predictor.
   */
  template <typename... Elems>
  self_type& btb(Elems... btb_impls);
};
} // namespace champsim

template <typename... Elems>
auto champsim::core_builder::branch_predictor(Elems... bp_impls) -> self_type&
{
  m_bp_impls = {bp_impls...};
  return *this;
}

template <typename... Elems>
auto champsim::core_builder::btb(Elems... btb_impls) -> self_type&
{
  m_btb_impls = {btb_impls...};
  return *this;
}
#endif
