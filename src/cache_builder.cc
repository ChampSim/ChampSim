
#include "cache_builder.h"


auto champsim::cache_builder::get_num_sets() const -> uint32_t
{
  uint32_t value = 0;
  if (m_sets.has_value())
    value = m_sets.value();
  else if (m_size.has_value() && m_ways.has_value())
    value = static_cast<uint32_t>(m_size.value().count() / (m_ways.value() * (1 << champsim::to_underlying(m_offset_bits)))); // casting the result of division
  else
    value = scaled_by_ul_size(m_sets_factor);
  return champsim::next_pow2(value);
}

auto champsim::cache_builder::get_num_ways() const -> uint32_t
{
  if (m_ways.has_value())
    return m_ways.value();
  if (m_size.has_value())
    return static_cast<uint32_t>(m_size.value().count() / (get_num_sets() * (1 << champsim::to_underlying(m_offset_bits)))); // casting the result of division
  return 1;
}

auto champsim::cache_builder::get_num_mshrs() const -> uint32_t
{
  auto default_count = (get_num_sets() * get_fill_latency() * static_cast<unsigned long>(champsim::to_underlying(get_fill_bandwidth())))
                       >> 4; // fill bandwidth should not be greater than 2^63
  return std::max(m_mshr_size.value_or(default_count), 1u);
}

auto champsim::cache_builder::get_tag_bandwidth() const -> champsim::bandwidth::maximum_type
{
  return std::max(m_max_tag.value_or(champsim::bandwidth::maximum_type{get_num_sets() >> 9}), champsim::bandwidth::maximum_type{1});
}

auto champsim::cache_builder::get_fill_bandwidth() const -> champsim::bandwidth::maximum_type
{
  return m_max_fill.value_or(get_tag_bandwidth());
}

auto champsim::cache_builder::get_hit_latency() const -> uint64_t
{
  if (m_hit_lat.has_value())
    return m_hit_lat.value();
  return get_total_latency() - get_fill_latency();
}

auto champsim::cache_builder::get_fill_latency() const -> uint64_t
{
  if (m_fill_lat.has_value())
    return m_fill_lat.value();
  return (get_total_latency() + 1) / 2;
}

auto champsim::cache_builder::get_total_latency() const -> uint64_t
{
  uint64_t latency{0};
  if (m_latency.has_value()) {
    latency = m_latency.value();
  } else {
    latency = std::llround(std::pow(get_num_sets() * get_num_ways(), 0.343) * 0.416);
  }
  return std::max(latency, uint64_t{2});
}

auto champsim::cache_builder::name(std::string name_) -> self_type&
{
  m_name = name_;
  return *this;
}

auto champsim::cache_builder::clock_period(champsim::chrono::picoseconds clock_period_) -> self_type&
{
  m_clock_period = clock_period_;
  return *this;
}

auto champsim::cache_builder::size(champsim::data::bytes size_) -> self_type&
{
  m_size = size_;
  return *this;
}

auto champsim::cache_builder::log2_size(uint64_t log2_size_) -> self_type&
{
  m_size = champsim::data::bytes{1 << log2_size_};
  return *this;
}

auto champsim::cache_builder::sets(uint32_t sets_) -> self_type&
{
  m_sets = sets_;
  return *this;
}

auto champsim::cache_builder::log2_sets(uint32_t log2_sets_) -> self_type&
{
  m_sets = 1 << log2_sets_;
  return *this;
}

auto champsim::cache_builder::sets_factor(double sets_factor_) -> self_type&
{
  m_sets_factor = sets_factor_;
  return *this;
}

auto champsim::cache_builder::ways(uint32_t ways_) -> self_type&
{
  m_ways = ways_;
  return *this;
}

auto champsim::cache_builder::log2_ways(uint32_t log2_ways_) -> self_type&
{
  m_ways = 1 << log2_ways_;
  return *this;
}

auto champsim::cache_builder::pq_size(uint32_t pq_size_) -> self_type&
{
  m_pq_size = pq_size_;
  return *this;
}

auto champsim::cache_builder::mshr_size(uint32_t mshr_size_) -> self_type&
{
  m_mshr_size = mshr_size_;
  return *this;
}

auto champsim::cache_builder::latency(uint64_t lat_) -> self_type&
{
  m_latency = lat_;
  return *this;
}

auto champsim::cache_builder::hit_latency(uint64_t hit_lat_) -> self_type&
{
  m_hit_lat = hit_lat_;
  return *this;
}

auto champsim::cache_builder::fill_latency(uint64_t fill_lat_) -> self_type&
{
  m_fill_lat = fill_lat_;
  return *this;
}

auto champsim::cache_builder::tag_bandwidth(champsim::bandwidth::maximum_type max_read_) -> self_type&
{
  m_max_tag = max_read_;
  return *this;
}

auto champsim::cache_builder::fill_bandwidth(champsim::bandwidth::maximum_type max_write_) -> self_type&
{
  m_max_fill = max_write_;
  return *this;
}

auto champsim::cache_builder::offset_bits(champsim::data::bits offset_bits_) -> self_type&
{
  m_offset_bits = offset_bits_;
  return *this;
}

auto champsim::cache_builder::log2_offset_bits(unsigned log2_offset_bits_) -> self_type&
{
  return offset_bits(champsim::data::bits{1ull << log2_offset_bits_});
}

auto champsim::cache_builder::set_prefetch_as_load() -> self_type&
{
  m_pref_load = true;
  return *this;
}

auto champsim::cache_builder::reset_prefetch_as_load() -> self_type&
{
  m_pref_load = false;
  return *this;
}

auto champsim::cache_builder::set_wq_checks_full_addr() -> self_type&
{
  m_wq_full_addr = true;
  return *this;
}

auto champsim::cache_builder::reset_wq_checks_full_addr() -> self_type&
{
  m_wq_full_addr = false;
  return *this;
}

auto champsim::cache_builder::set_virtual_prefetch() -> self_type&
{
  m_va_pref = true;
  return *this;
}

auto champsim::cache_builder::reset_virtual_prefetch() -> self_type&
{
  m_va_pref = false;
  return *this;
}

auto champsim::cache_builder::upper_levels(std::vector<champsim::channel*>&& uls_) -> self_type&
{
  m_uls = std::move(uls_);
  return *this;
}

auto champsim::cache_builder::lower_level(champsim::channel* ll_) -> self_type&
{
  m_ll = ll_;
  return *this;
}

auto champsim::cache_builder::lower_translate(champsim::channel* lt_) -> self_type&
{
  m_lt = lt_;
  return *this;
}

