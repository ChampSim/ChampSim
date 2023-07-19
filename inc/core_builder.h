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

#include <limits>

#include "chrono.h"

class CACHE;
class O3_CPU;
namespace champsim
{
class channel;
class core_builder_conversion_tag
{
};
template <unsigned long long B_FLAG = 0, unsigned long long T_FLAG = 0>
class core_builder
{
  using self_type = core_builder<B_FLAG, T_FLAG>;

  uint32_t m_cpu{};
  champsim::chrono::picoseconds m_clock_period{250};
  std::size_t m_dib_set{};
  std::size_t m_dib_way{};
  std::size_t m_dib_window{};
  std::size_t m_ifetch_buffer_size{};
  std::size_t m_decode_buffer_size{};
  std::size_t m_dispatch_buffer_size{};
  std::size_t m_rob_size{};
  std::size_t m_lq_size{};
  std::size_t m_sq_size{};
  unsigned m_fetch_width{};
  unsigned m_decode_width{};
  unsigned m_dispatch_width{};
  unsigned m_schedule_width{};
  unsigned m_execute_width{};
  unsigned m_lq_width{};
  unsigned m_sq_width{};
  unsigned m_retire_width{};
  unsigned m_mispredict_penalty{};
  unsigned m_decode_latency{};
  unsigned m_dispatch_latency{};
  unsigned m_schedule_latency{};
  unsigned m_execute_latency{};

  CACHE* m_l1i{};
  long int m_l1i_bw{};
  long int m_l1d_bw{};
  champsim::channel* m_fetch_queues{};
  champsim::channel* m_data_queues{};

  friend class ::O3_CPU;

  template <unsigned long long OTHER_B, unsigned long long OTHER_T>
  friend class core_builder;

  template <unsigned long long OTHER_B, unsigned long long OTHER_T>
  core_builder(core_builder_conversion_tag /*tag*/, const core_builder<OTHER_B, OTHER_T>& other)
      : m_cpu(other.m_cpu), m_clock_period(other.m_clock_period), m_dib_set(other.m_dib_set), m_dib_way(other.m_dib_way), m_dib_window(other.m_dib_window),
        m_ifetch_buffer_size(other.m_ifetch_buffer_size), m_decode_buffer_size(other.m_decode_buffer_size),
        m_dispatch_buffer_size(other.m_dispatch_buffer_size), m_rob_size(other.m_rob_size), m_lq_size(other.m_lq_size), m_sq_size(other.m_sq_size),
        m_fetch_width(other.m_fetch_width), m_decode_width(other.m_decode_width), m_dispatch_width(other.m_dispatch_width),
        m_schedule_width(other.m_schedule_width), m_execute_width(other.m_execute_width), m_lq_width(other.m_lq_width), m_sq_width(other.m_sq_width),
        m_retire_width(other.m_retire_width), m_mispredict_penalty(other.m_mispredict_penalty), m_decode_latency(other.m_decode_latency),
        m_dispatch_latency(other.m_dispatch_latency), m_schedule_latency(other.m_schedule_latency), m_execute_latency(other.m_execute_latency),
        m_l1i(other.m_l1i), m_l1i_bw(other.m_l1i_bw), m_l1d_bw(other.m_l1d_bw), m_fetch_queues(other.m_fetch_queues), m_data_queues(other.m_data_queues)
  {
  }

public:
  core_builder() = default;

  self_type& index(uint32_t cpu_);
  self_type& clock_period(champsim::chrono::picoseconds clock_period_);
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

  template <unsigned long long B>
  core_builder<B, T_FLAG> branch_predictor();
  template <unsigned long long T>
  core_builder<B_FLAG, T> btb();
};
} // namespace champsim

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::index(uint32_t cpu_) -> self_type&
{
  m_cpu = cpu_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::clock_period(champsim::chrono::picoseconds clock_period_) -> self_type&
{
  m_clock_period = clock_period_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::dib_set(std::size_t dib_set_) -> self_type&
{
  m_dib_set = dib_set_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::dib_way(std::size_t dib_way_) -> self_type&
{
  m_dib_way = dib_way_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::dib_window(std::size_t dib_window_) -> self_type&
{
  m_dib_window = dib_window_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::ifetch_buffer_size(std::size_t ifetch_buffer_size_) -> self_type&
{
  m_ifetch_buffer_size = ifetch_buffer_size_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::decode_buffer_size(std::size_t decode_buffer_size_) -> self_type&
{
  m_decode_buffer_size = decode_buffer_size_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::dispatch_buffer_size(std::size_t dispatch_buffer_size_) -> self_type&
{
  m_dispatch_buffer_size = dispatch_buffer_size_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::rob_size(std::size_t rob_size_) -> self_type&
{
  m_rob_size = rob_size_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::lq_size(std::size_t lq_size_) -> self_type&
{
  m_lq_size = lq_size_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::sq_size(std::size_t sq_size_) -> self_type&
{
  m_sq_size = sq_size_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::fetch_width(unsigned fetch_width_) -> self_type&
{
  m_fetch_width = fetch_width_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::decode_width(unsigned decode_width_) -> self_type&
{
  m_decode_width = decode_width_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::dispatch_width(unsigned dispatch_width_) -> self_type&
{
  m_dispatch_width = dispatch_width_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::schedule_width(unsigned schedule_width_) -> self_type&
{
  m_schedule_width = schedule_width_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::execute_width(unsigned execute_width_) -> self_type&
{
  m_execute_width = execute_width_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::lq_width(unsigned lq_width_) -> self_type&
{
  m_lq_width = lq_width_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::sq_width(unsigned sq_width_) -> self_type&
{
  m_sq_width = sq_width_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::retire_width(unsigned retire_width_) -> self_type&
{
  m_retire_width = retire_width_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::mispredict_penalty(unsigned mispredict_penalty_) -> self_type&
{
  m_mispredict_penalty = mispredict_penalty_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::decode_latency(unsigned decode_latency_) -> self_type&
{
  m_decode_latency = decode_latency_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::dispatch_latency(unsigned dispatch_latency_) -> self_type&
{
  m_dispatch_latency = dispatch_latency_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::schedule_latency(unsigned schedule_latency_) -> self_type&
{
  m_schedule_latency = schedule_latency_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::execute_latency(unsigned execute_latency_) -> self_type&
{
  m_execute_latency = execute_latency_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::l1i(CACHE* l1i_) -> self_type&
{
  m_l1i = l1i_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::l1i_bandwidth(long int l1i_bw_) -> self_type&
{
  m_l1i_bw = l1i_bw_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::l1d_bandwidth(long int l1d_bw_) -> self_type&
{
  m_l1d_bw = l1d_bw_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::fetch_queues(champsim::channel* fetch_queues_) -> self_type&
{
  m_fetch_queues = fetch_queues_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
auto champsim::core_builder<B_FLAG, T_FLAG>::data_queues(champsim::channel* data_queues_) -> self_type&
{
  m_data_queues = data_queues_;
  return *this;
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
template <unsigned long long B>
champsim::core_builder<B, T_FLAG> champsim::core_builder<B_FLAG, T_FLAG>::branch_predictor()
{
  return champsim::core_builder<B, T_FLAG>{core_builder_conversion_tag{}, *this};
}

template <unsigned long long B_FLAG, unsigned long long T_FLAG>
template <unsigned long long T>
champsim::core_builder<B_FLAG, T> champsim::core_builder<B_FLAG, T_FLAG>::btb()
{
  return champsim::core_builder<B_FLAG, T>{core_builder_conversion_tag{}, *this};
}

#endif
