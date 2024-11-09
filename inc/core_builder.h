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

#include "chrono.h"

class CACHE;
class O3_CPU;
namespace champsim
{
class channel;
template <typename...>
class core_builder_module_type_holder
{
};
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
};
} // namespace detail

template <typename B = core_builder_module_type_holder<>, typename T = core_builder_module_type_holder<>>
class core_builder : public detail::core_builder_base
{
  using self_type = core_builder<B, T>;

  friend class ::O3_CPU;

  template <typename OTHER_B, typename OTHER_T>
  friend class core_builder;

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
  template <typename... Bs>
  core_builder<core_builder_module_type_holder<Bs...>, T> branch_predictor();

  /**
   * Specify the branch target predictor.
   */
  template <typename... Ts>
  core_builder<B, core_builder_module_type_holder<Ts...>> btb();
};
} // namespace champsim

template <typename B, typename T>
auto champsim::core_builder<B, T>::index(uint32_t cpu_) -> self_type&
{
  m_cpu = cpu_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::clock_period(champsim::chrono::picoseconds clock_period_) -> self_type&
{
  m_clock_period = clock_period_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::dib_set(std::size_t dib_set_) -> self_type&
{
  m_dib_set = dib_set_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::dib_way(std::size_t dib_way_) -> self_type&
{
  m_dib_way = dib_way_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::dib_window(std::size_t dib_window_) -> self_type&
{
  m_dib_window = dib_window_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::ifetch_buffer_size(std::size_t ifetch_buffer_size_) -> self_type&
{
  m_ifetch_buffer_size = ifetch_buffer_size_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::decode_buffer_size(std::size_t decode_buffer_size_) -> self_type&
{
  m_decode_buffer_size = decode_buffer_size_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::dispatch_buffer_size(std::size_t dispatch_buffer_size_) -> self_type&
{
  m_dispatch_buffer_size = dispatch_buffer_size_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::register_file_size(std::size_t register_file_size_) -> self_type&
{
  m_register_file_size = register_file_size_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::rob_size(std::size_t rob_size_) -> self_type&
{
  m_rob_size = rob_size_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::dib_hit_buffer_size(std::size_t dib_hit_buffer_size_) -> self_type&
{
  m_dib_hit_buffer_size = dib_hit_buffer_size_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::lq_size(std::size_t lq_size_) -> self_type&
{
  m_lq_size = lq_size_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::sq_size(std::size_t sq_size_) -> self_type&
{
  m_sq_size = sq_size_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::fetch_width(champsim::bandwidth::maximum_type fetch_width_) -> self_type&
{
  m_fetch_width = fetch_width_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::decode_width(champsim::bandwidth::maximum_type decode_width_) -> self_type&
{
  m_decode_width = decode_width_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::dispatch_width(champsim::bandwidth::maximum_type dispatch_width_) -> self_type&
{
  m_dispatch_width = dispatch_width_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::schedule_width(champsim::bandwidth::maximum_type schedule_width_) -> self_type&
{
  m_schedule_width = schedule_width_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::execute_width(champsim::bandwidth::maximum_type execute_width_) -> self_type&
{
  m_execute_width = execute_width_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::lq_width(champsim::bandwidth::maximum_type lq_width_) -> self_type&
{
  m_lq_width = lq_width_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::sq_width(champsim::bandwidth::maximum_type sq_width_) -> self_type&
{
  m_sq_width = sq_width_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::retire_width(champsim::bandwidth::maximum_type retire_width_) -> self_type&
{
  m_retire_width = retire_width_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::dib_inorder_width(champsim::bandwidth::maximum_type dib_inorder_width_) -> self_type&
{
  m_dib_inorder_width = dib_inorder_width_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::mispredict_penalty(unsigned mispredict_penalty_) -> self_type&
{
  m_mispredict_penalty = mispredict_penalty_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::decode_latency(unsigned decode_latency_) -> self_type&
{
  m_decode_latency = decode_latency_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::dib_hit_latency(unsigned dib_hit_latency_) -> self_type&
{
  m_dib_hit_latency = dib_hit_latency_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::dispatch_latency(unsigned dispatch_latency_) -> self_type&
{
  m_dispatch_latency = dispatch_latency_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::schedule_latency(unsigned schedule_latency_) -> self_type&
{
  m_schedule_latency = schedule_latency_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::execute_latency(unsigned execute_latency_) -> self_type&
{
  m_execute_latency = execute_latency_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::l1i(CACHE* l1i_) -> self_type&
{
  m_l1i = l1i_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::l1i_bandwidth(champsim::bandwidth::maximum_type l1i_bw_) -> self_type&
{
  m_l1i_bw = l1i_bw_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::l1d_bandwidth(champsim::bandwidth::maximum_type l1d_bw_) -> self_type&
{
  m_l1d_bw = l1d_bw_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::fetch_queues(champsim::channel* fetch_queues_) -> self_type&
{
  m_fetch_queues = fetch_queues_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::data_queues(champsim::channel* data_queues_) -> self_type&
{
  m_data_queues = data_queues_;
  return *this;
}

template <typename B, typename T>
template <typename... Bs>
auto champsim::core_builder<B, T>::branch_predictor() -> champsim::core_builder<core_builder_module_type_holder<Bs...>, T>
{
  return champsim::core_builder<core_builder_module_type_holder<Bs...>, T>{*this};
}

template <typename B, typename T>
template <typename... Ts>
auto champsim::core_builder<B, T>::btb() -> champsim::core_builder<B, core_builder_module_type_holder<Ts...>>
{
  return champsim::core_builder<B, core_builder_module_type_holder<Ts...>>{*this};
}

#endif
