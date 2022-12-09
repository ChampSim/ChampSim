#ifndef ADDRESS_H
#define ADDRESS_H

#include <algorithm>
#include <cassert>
#include <ios>
#include <iostream>

#include "champsim_constants.h"
#include "util/bits.h"

namespace champsim {
template <std::size_t, std::size_t>
class address_slice;
inline constexpr std::size_t dynamic_extent = std::numeric_limits<std::size_t>::max();

// Convenience definitions
using address = address_slice<std::numeric_limits<uint64_t>::digits-1, 0>;
using block_number = address_slice<std::numeric_limits<uint64_t>::digits-1, LOG2_BLOCK_SIZE>;
using block_offset = address_slice<LOG2_BLOCK_SIZE, 0>;
using page_number = address_slice<std::numeric_limits<uint64_t>::digits-1, LOG2_PAGE_SIZE>;
using page_offset = address_slice<LOG2_PAGE_SIZE, 0>;

template <std::size_t UP, std::size_t LOW>
auto offset(address_slice<UP, LOW> base, address_slice<UP, LOW> other) -> typename address_slice<UP, LOW>::difference_type;

template <std::size_t UP, std::size_t LOW>
auto splice(address_slice<UP, LOW> upper, address_slice<UP, LOW> lower, std::size_t bits) -> address_slice<UP, LOW>;

template <>
class address_slice<dynamic_extent, dynamic_extent>;

template <std::size_t UP, std::size_t LOW>
class address_slice
{
  constexpr static std::size_t upper{UP};
  constexpr static std::size_t lower{LOW};
  using self_type = address_slice<upper, lower>;

  template <std::size_t U, std::size_t L> friend class address_slice;

  using underlying_type = uint64_t;
  underlying_type value{};

  static_assert(LOW <= UP);
  static_assert(UP < std::numeric_limits<underlying_type>::digits);
  static_assert(LOW < std::numeric_limits<underlying_type>::digits);

  friend std::ostream& operator<<(std::ostream& stream, const self_type &addr)
  {
    stream << "0x" << std::hex << addr.template to<underlying_type>() << std::dec;
    return stream;
  }

  public:
  using difference_type = int64_t;

  address_slice() = default; // TODO remove this
  explicit address_slice(underlying_type val) : value((val & bitmask(upper, lower)) >> lower) {}

  template <std::size_t other_up, std::size_t other_low>
  explicit address_slice(address_slice<other_up, other_low> val) : value((val.value & bitmask(upper, lower)) >> lower)
  {
    // For now, this slice is must be a contraction
    static_assert(other_up >= upper);
    static_assert(other_low <= lower);
  }

  template <typename T>
  T to() const
  {
    //todo check preconditions
    static_assert(std::is_integral_v<T>);
    assert(value <= std::numeric_limits<T>::max());
    assert(static_cast<T>(value) >= std::numeric_limits<T>::min());
    return static_cast<T>(value);
  }

  address to_address() const
  {
    return champsim::address{value << lower};
  }

  friend auto offset<>(self_type base, self_type other) -> difference_type;
  friend auto splice<>(self_type upper, self_type lower, std::size_t bits) -> self_type;

  bool operator==(self_type other) const { return value == other.value; }
  bool operator!=(self_type other) const { return !(*this == other); }
  bool operator< (self_type other) const { return value < other.value; }
  bool operator<=(self_type other) const { return *this < other || *this == other; }
  bool operator> (self_type other) const { return !(value <= other.value); }
  bool operator>=(self_type other) const { return *this > other || *this == other; }

  self_type& operator+=(difference_type delta)
  {
    auto newval = value + delta;
    //assert((newval & bitmask(upper, lower)) == newval); //FIXME
    value = newval;
    return *this;
  }
  self_type& operator-=(difference_type delta) { return *this += (-delta); }
  self_type operator+(difference_type delta) { self_type retval{*this}; retval += delta; return retval; }
  self_type operator-(difference_type delta) { self_type retval{*this}; retval -= delta; return retval; }

  template <std::size_t slice_upper, std::size_t slice_lower>
  address_slice<slice_upper + lower, slice_lower + lower> slice() const {
    static_assert(slice_lower <= (upper - lower));
    static_assert(slice_upper <= (upper - lower));
    return address_slice<slice_upper + lower, slice_lower + lower>{to_address()};
  }
  template <std::size_t new_lower>
  address_slice<upper-lower, new_lower> slice_upper() const { return slice<upper-lower, new_lower>(); }
  template <std::size_t new_upper>
  address_slice<new_upper, 0> slice_lower() const { return slice<new_upper, 0>(); }

  address_slice<dynamic_extent, dynamic_extent> slice(std::size_t new_upper, std::size_t new_lower) const;
  address_slice<dynamic_extent, dynamic_extent> slice_upper(std::size_t new_lower) const;
  address_slice<dynamic_extent, dynamic_extent> slice_lower(std::size_t new_upper) const;
};

template <>
class address_slice<dynamic_extent, dynamic_extent>
{
  std::size_t upper{};
  std::size_t lower{};

  using underlying_type = uint64_t;
  constexpr static int bits = std::numeric_limits<underlying_type>::digits;
  underlying_type value{};

  friend std::ostream& operator<<(std::ostream& stream, const address_slice<dynamic_extent, dynamic_extent> &addr)
  {
    stream << "0x" << std::hex << addr.to<underlying_type>() << std::dec;
    return stream;
  }

  public:
  explicit address_slice(champsim::address val) : address_slice(bits-1, 0, val) {}
  address_slice(std::size_t up, std::size_t low, champsim::address val) : upper(up), lower(low), value((val.value & bitmask(up, low)) >> low)
  {
    assert(up >= low);
    assert(up <= bits);
    assert(low <= bits);
  }

  template <typename T>
  T to() const
  {
    static_assert(std::is_integral_v<T>);
    assert(value <= std::numeric_limits<T>::max());
    assert(static_cast<T>(value) >= std::numeric_limits<T>::min());
    return static_cast<T>(value);
  }

  address to_address() const
  {
    return champsim::address{value << lower};
  }

  template <std::size_t other_up, std::size_t other_low>
  bool operator==(address_slice<other_up, other_low> other) const
  {
    assert(this->upper == other.upper);
    assert(this->lower == other.lower);
    return value == other.value;
  }
  template <std::size_t other_up, std::size_t other_low>
  bool operator< (address_slice<other_up, other_low> other) const
  {
    assert(this->upper == other.upper);
    assert(this->lower == other.lower);
    return value < other.value;
  }

  template <std::size_t other_up, std::size_t other_low>
  bool operator!=(address_slice<other_up, other_low> other) const { return !(*this == other); }
  template <std::size_t other_up, std::size_t other_low>
  bool operator<=(address_slice<other_up, other_low> other) const { return *this < other || *this == other; }
  template <std::size_t other_up, std::size_t other_low>
  bool operator> (address_slice<other_up, other_low> other) const { return !(value <= other.value); }
  template <std::size_t other_up, std::size_t other_low>
  bool operator>=(address_slice<other_up, other_low> other) const { return *this > other || *this == other; }

  address_slice<dynamic_extent, dynamic_extent> slice(std::size_t slice_upper, std::size_t slice_lower) const {
    assert(slice_lower <= (upper - lower));
    assert(slice_upper <= (upper - lower));
    return address_slice<dynamic_extent, dynamic_extent>{slice_upper + lower, slice_lower + lower, to_address()};
  }
  address_slice<dynamic_extent, dynamic_extent> slice_upper(std::size_t new_lower) const { return slice(upper-lower, new_lower); }
  address_slice<dynamic_extent, dynamic_extent> slice_lower(std::size_t new_upper) const { return slice(new_upper, 0); }
};

address_slice(std::size_t, std::size_t, champsim::address) -> address_slice<dynamic_extent, dynamic_extent>;

template <std::size_t UP, std::size_t LOW>
auto address_slice<UP, LOW>::slice(std::size_t new_upper, std::size_t new_lower) const -> address_slice<dynamic_extent, dynamic_extent>
{
  assert(new_lower <= (upper - lower));
  assert(new_upper <= (upper - lower));
  return address_slice<dynamic_extent, dynamic_extent>{new_upper, new_lower, to_address()};
}

template <std::size_t UP, std::size_t LOW>
auto address_slice<UP, LOW>::slice_upper(std::size_t new_lower) const -> address_slice<dynamic_extent, dynamic_extent>
{
  return slice(upper-lower, new_lower);
}

template <std::size_t UP, std::size_t LOW>
auto address_slice<UP, LOW>::slice_lower(std::size_t new_upper) const -> address_slice<dynamic_extent, dynamic_extent>
{
  return slice(new_upper, 0);
}

template <std::size_t UP, std::size_t LOW>
auto offset(address_slice<UP, LOW> base, address_slice<UP, LOW> other) -> typename address_slice<UP, LOW>::difference_type
{
  using underlying_type = typename address_slice<UP, LOW>::underlying_type;
  using difference_type = typename address_slice<UP, LOW>::difference_type;

  underlying_type abs_diff = (base.value > other.value) ? (base.value - other.value) : (other.value - base.value);
  assert(abs_diff <= std::numeric_limits<difference_type>::max());
  return (base.value > other.value) ? -static_cast<difference_type>(abs_diff) : static_cast<difference_type>(abs_diff);
}

template <std::size_t UP, std::size_t LOW>
auto splice(address_slice<UP, LOW> upper, address_slice<UP, LOW> lower, std::size_t bits) -> address_slice<UP, LOW>
{
  return address_slice<UP, LOW>{splice_bits(upper.value, lower.value, bits)};
}
}

#endif
