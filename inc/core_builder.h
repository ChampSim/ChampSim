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
  double m_freq_scale{1};
  std::size_t m_dib_set{1};
  std::size_t m_dib_way{1};
  std::size_t m_dib_window{1};
  std::size_t m_ifetch_buffer_size{1};
  std::size_t m_decode_buffer_size{1};
  std::size_t m_dispatch_buffer_size{1};
  std::size_t m_rob_size{1};
  std::size_t m_lq_size{1};
  std::size_t m_sq_size{1};
  unsigned m_fetch_width{1};
  unsigned m_decode_width{1};
  unsigned m_dispatch_width{1};
  unsigned m_schedule_width{1};
  unsigned m_execute_width{1};
  unsigned m_lq_width{1};
  unsigned m_sq_width{1};
  unsigned m_retire_width{1};
  unsigned m_mispredict_penalty{};
  unsigned m_decode_latency{};
  unsigned m_dispatch_latency{};
  unsigned m_schedule_latency{};
  unsigned m_execute_latency{};

  CACHE* m_l1i{};
  long int m_l1i_bw{1};
  long int m_l1d_bw{1};
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
  self_type& frequency(double freq_scale_);
  self_type& dib_set(std::size_t dib_set_);
  self_type& dib_way(std::size_t dib_way_);
  self_type& dib_window(std::size_t dib_window_);
  self_type& ifetch_buffer_size(std::size_t ifetch_buffer_size_);
  self_type& decode_buffer_size(std::size_t decode_buffer_size_);
  self_type& dispatch_buffer_size(std::size_t dispatch_buffer_size_);
  self_type& rob_size(std::size_t rob_size_);
  self_type& lq_size(std::size_t lq_size_);
  self_type& sq_size(std::size_t sq_size_);
  self_type& fetch_width(unsigned fetch_width_);
  self_type& decode_width(unsigned decode_width_);
  self_type& dispatch_width(unsigned dispatch_width_);
  self_type& schedule_width(unsigned schedule_width_);
  self_type& execute_width(unsigned execute_width_);
  self_type& lq_width(unsigned lq_width_);
  self_type& sq_width(unsigned sq_width_);
  self_type& retire_width(unsigned retire_width_);
  self_type& mispredict_penalty(unsigned mispredict_penalty_);
  self_type& decode_latency(unsigned decode_latency_);
  self_type& dispatch_latency(unsigned dispatch_latency_);
  self_type& schedule_latency(unsigned schedule_latency_);
  self_type& execute_latency(unsigned execute_latency_);
  self_type& l1i(CACHE* l1i_);
  self_type& l1i_bandwidth(long int l1i_bw_);
  self_type& l1d_bandwidth(long int l1d_bw_);
  self_type& fetch_queues(champsim::channel* fetch_queues_);
  self_type& data_queues(champsim::channel* data_queues_);

  template <typename... Bs>
  core_builder<core_builder_module_type_holder<Bs...>, T> branch_predictor();
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
auto champsim::core_builder<B, T>::frequency(double freq_scale_) -> self_type&
{
  m_freq_scale = freq_scale_;
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
auto champsim::core_builder<B, T>::rob_size(std::size_t rob_size_) -> self_type&
{
  m_rob_size = rob_size_;
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
auto champsim::core_builder<B, T>::fetch_width(unsigned fetch_width_) -> self_type&
{
  m_fetch_width = fetch_width_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::decode_width(unsigned decode_width_) -> self_type&
{
  m_decode_width = decode_width_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::dispatch_width(unsigned dispatch_width_) -> self_type&
{
  m_dispatch_width = dispatch_width_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::schedule_width(unsigned schedule_width_) -> self_type&
{
  m_schedule_width = schedule_width_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::execute_width(unsigned execute_width_) -> self_type&
{
  m_execute_width = execute_width_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::lq_width(unsigned lq_width_) -> self_type&
{
  m_lq_width = lq_width_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::sq_width(unsigned sq_width_) -> self_type&
{
  m_sq_width = sq_width_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::retire_width(unsigned retire_width_) -> self_type&
{
  m_retire_width = retire_width_;
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
auto champsim::core_builder<B, T>::l1i_bandwidth(long int l1i_bw_) -> self_type&
{
  m_l1i_bw = l1i_bw_;
  return *this;
}

template <typename B, typename T>
auto champsim::core_builder<B, T>::l1d_bandwidth(long int l1d_bw_) -> self_type&
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
