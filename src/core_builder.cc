#include "core_builder.h"

auto champsim::core_builder::index(uint32_t cpu_) -> self_type&
{
  m_cpu = cpu_;
  return *this;
}


auto champsim::core_builder::clock_period(champsim::chrono::picoseconds clock_period_) -> self_type&
{
  m_clock_period = clock_period_;
  return *this;
}


auto champsim::core_builder::dib_set(std::size_t dib_set_) -> self_type&
{
  m_dib_set = dib_set_;
  return *this;
}


auto champsim::core_builder::dib_way(std::size_t dib_way_) -> self_type&
{
  m_dib_way = dib_way_;
  return *this;
}


auto champsim::core_builder::dib_window(std::size_t dib_window_) -> self_type&
{
  m_dib_window = dib_window_;
  return *this;
}


auto champsim::core_builder::ifetch_buffer_size(std::size_t ifetch_buffer_size_) -> self_type&
{
  m_ifetch_buffer_size = ifetch_buffer_size_;
  return *this;
}


auto champsim::core_builder::decode_buffer_size(std::size_t decode_buffer_size_) -> self_type&
{
  m_decode_buffer_size = decode_buffer_size_;
  return *this;
}


auto champsim::core_builder::dispatch_buffer_size(std::size_t dispatch_buffer_size_) -> self_type&
{
  m_dispatch_buffer_size = dispatch_buffer_size_;
  return *this;
}


auto champsim::core_builder::register_file_size(std::size_t register_file_size_) -> self_type&
{
  m_register_file_size = register_file_size_;
  return *this;
}


auto champsim::core_builder::rob_size(std::size_t rob_size_) -> self_type&
{
  m_rob_size = rob_size_;
  return *this;
}


auto champsim::core_builder::dib_hit_buffer_size(std::size_t dib_hit_buffer_size_) -> self_type&
{
  m_dib_hit_buffer_size = dib_hit_buffer_size_;
  return *this;
}


auto champsim::core_builder::lq_size(std::size_t lq_size_) -> self_type&
{
  m_lq_size = lq_size_;
  return *this;
}


auto champsim::core_builder::sq_size(std::size_t sq_size_) -> self_type&
{
  m_sq_size = sq_size_;
  return *this;
}


auto champsim::core_builder::fetch_width(champsim::bandwidth::maximum_type fetch_width_) -> self_type&
{
  m_fetch_width = fetch_width_;
  return *this;
}


auto champsim::core_builder::decode_width(champsim::bandwidth::maximum_type decode_width_) -> self_type&
{
  m_decode_width = decode_width_;
  return *this;
}


auto champsim::core_builder::dispatch_width(champsim::bandwidth::maximum_type dispatch_width_) -> self_type&
{
  m_dispatch_width = dispatch_width_;
  return *this;
}


auto champsim::core_builder::schedule_width(champsim::bandwidth::maximum_type schedule_width_) -> self_type&
{
  m_schedule_width = schedule_width_;
  return *this;
}


auto champsim::core_builder::execute_width(champsim::bandwidth::maximum_type execute_width_) -> self_type&
{
  m_execute_width = execute_width_;
  return *this;
}


auto champsim::core_builder::lq_width(champsim::bandwidth::maximum_type lq_width_) -> self_type&
{
  m_lq_width = lq_width_;
  return *this;
}


auto champsim::core_builder::sq_width(champsim::bandwidth::maximum_type sq_width_) -> self_type&
{
  m_sq_width = sq_width_;
  return *this;
}


auto champsim::core_builder::retire_width(champsim::bandwidth::maximum_type retire_width_) -> self_type&
{
  m_retire_width = retire_width_;
  return *this;
}


auto champsim::core_builder::dib_inorder_width(champsim::bandwidth::maximum_type dib_inorder_width_) -> self_type&
{
  m_dib_inorder_width = dib_inorder_width_;
  return *this;
}


auto champsim::core_builder::mispredict_penalty(unsigned mispredict_penalty_) -> self_type&
{
  m_mispredict_penalty = mispredict_penalty_;
  return *this;
}


auto champsim::core_builder::decode_latency(unsigned decode_latency_) -> self_type&
{
  m_decode_latency = decode_latency_;
  return *this;
}


auto champsim::core_builder::dib_hit_latency(unsigned dib_hit_latency_) -> self_type&
{
  m_dib_hit_latency = dib_hit_latency_;
  return *this;
}


auto champsim::core_builder::dispatch_latency(unsigned dispatch_latency_) -> self_type&
{
  m_dispatch_latency = dispatch_latency_;
  return *this;
}


auto champsim::core_builder::schedule_latency(unsigned schedule_latency_) -> self_type&
{
  m_schedule_latency = schedule_latency_;
  return *this;
}


auto champsim::core_builder::execute_latency(unsigned execute_latency_) -> self_type&
{
  m_execute_latency = execute_latency_;
  return *this;
}


auto champsim::core_builder::l1i(CACHE* l1i_) -> self_type&
{
  m_l1i = l1i_;
  return *this;
}


auto champsim::core_builder::l1i_bandwidth(champsim::bandwidth::maximum_type l1i_bw_) -> self_type&
{
  m_l1i_bw = l1i_bw_;
  return *this;
}


auto champsim::core_builder::l1d_bandwidth(champsim::bandwidth::maximum_type l1d_bw_) -> self_type&
{
  m_l1d_bw = l1d_bw_;
  return *this;
}


auto champsim::core_builder::fetch_queues(champsim::channel* fetch_queues_) -> self_type&
{
  m_fetch_queues = fetch_queues_;
  return *this;
}


auto champsim::core_builder::data_queues(champsim::channel* data_queues_) -> self_type&
{
  m_data_queues = data_queues_;
  return *this;
}

