#include "address.h"

#include "champsim_constants.h"

champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> champsim::address::slice(std::size_t upper, std::size_t lower) const
{
  return address_slice<dynamic_extent, dynamic_extent>{upper, lower, *this};
}

champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> champsim::address::slice_upper(std::size_t lower) const
{
  return slice(std::numeric_limits<underlying_type>::digits, lower);
}

champsim::address_slice<champsim::dynamic_extent, champsim::dynamic_extent> champsim::address::slice_lower(std::size_t upper) const
{
  return slice(upper, 0);
}

template <std::size_t LOW>
auto champsim::address::slice_upper() const -> champsim::address_slice<bits, LOW>
{
  return slice<bits, LOW>();
}

template <std::size_t UP>
auto champsim::address::slice_lower() const -> champsim::address_slice<UP, 0>
{
  return slice<UP, 0>();
}

auto champsim::address::block_address() const -> champsim::address_slice<bits, LOG2_BLOCK_SIZE>
{
  return slice_upper<LOG2_BLOCK_SIZE>();
}

auto champsim::address::page_address() const -> champsim::address_slice<bits, LOG2_PAGE_SIZE>
{
  return slice_upper<LOG2_PAGE_SIZE>();
}

bool champsim::address::is_block_address() const
{
  return *this == block_address().to_address();
}

bool champsim::address::is_page_address() const
{
  return *this == page_address().to_address();
}

auto champsim::address::offset(champsim::address base, champsim::address other) -> champsim::address::difference_type
{
  underlying_type abs_diff = (base.value > other.value) ? (base.value - other.value) : (other.value - base.value);
  assert(abs_diff <= std::numeric_limits<difference_type>::max());
  return (base.value > other.value) ? -static_cast<difference_type>(abs_diff) : static_cast<difference_type>(abs_diff);
}

champsim::address champsim::address::splice(champsim::address upper, champsim::address lower, std::size_t bits)
{
  return address{splice_bits(upper.value, lower.value, bits)};
}
