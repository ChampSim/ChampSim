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

class address
{
  public:
    using underlying_type = uint64_t;
    using difference_type = int64_t;
    constexpr static int bits = std::numeric_limits<underlying_type>::digits;

    address() = default;
    explicit address(underlying_type addr) : value(addr) {}

    address_slice<dynamic_extent, dynamic_extent> slice(std::size_t upper, std::size_t lower) const;
    address_slice<dynamic_extent, dynamic_extent> slice_upper(std::size_t lower) const;
    address_slice<dynamic_extent, dynamic_extent> slice_lower(std::size_t upper) const;

    template <std::size_t UP, std::size_t LOW> address_slice<UP, LOW> slice() const;
    template <std::size_t LOW>                 address_slice<bits, LOW> slice_upper() const;
    template <std::size_t UP>                  address_slice<UP, 0> slice_lower() const;

    address_slice<bits, LOG2_BLOCK_SIZE> block_address() const;
    address_slice<bits, LOG2_PAGE_SIZE> page_address() const;
    bool is_block_address() const;
    bool is_page_address() const;

    static difference_type offset(address base, address other);
    static address splice(address upper, address lower, std::size_t bits);

    bool operator==(address other) const { return value == other.value; }
    bool operator!=(address other) const { return !(*this == other); }
    bool operator< (address other) const { return value < other.value; }
    bool operator<=(address other) const { return *this < other || *this == other; }
    bool operator> (address other) const { return !(value <= other.value); }
    bool operator>=(address other) const { return *this > other || *this == other; }

    address& operator+=(difference_type delta) { value += delta; return *this; }
    address operator+(difference_type delta) { address retval{*this}; retval += delta; return retval; }
    address& operator-=(difference_type delta) { value -= delta; return *this; }
    address operator-(difference_type delta) { address retval{*this}; retval -= delta; return retval; }

  private:
    underlying_type value{};

    template <std::size_t, std::size_t> friend class address_slice;
    friend std::ostream& operator<<(std::ostream& stream, const champsim::address &addr)
    {
      stream << "0x" << std::hex << addr.value << std::dec;
      return stream;
    }
};

template <>
class address_slice<dynamic_extent, dynamic_extent>;

template <std::size_t UP, std::size_t LOW>
class address_slice
{
  constexpr static std::size_t upper{UP};
  constexpr static std::size_t lower{LOW};

  friend class address;
  using underlying_type = address::underlying_type;
  constexpr static int bits = std::numeric_limits<underlying_type>::digits;
  underlying_type value{};

  static_assert(UP >= LOW);
  static_assert(UP <= bits);
  static_assert(LOW <= bits);

  friend std::ostream& operator<<(std::ostream& stream, const address_slice<UP, LOW> &addr)
  {
    stream << "0x" << std::hex << addr.to<underlying_type>() << std::dec;
    return stream;
  }

  public:
  explicit address_slice(champsim::address val) : value(val.value) {}

  template <typename T>
  T to() const
  {
    //todo check preconditions
    static_assert(std::is_integral_v<T>);
    underlying_type raw_val = (value >> lower) & champsim::bitmask(upper-lower);
    assert(raw_val <= std::numeric_limits<T>::max());
    assert(static_cast<T>(raw_val) >= std::numeric_limits<T>::min());
    return static_cast<T>(raw_val);
  }

  address to_address() const
  {
    return champsim::address{value & champsim::bitmask(upper, lower)};
  }

  bool operator==(address_slice<upper, lower> other) const
  {
    return (value & bitmask(upper, lower)) == (other.value & bitmask(upper, lower));
  }

  template <std::size_t other_up, std::size_t other_low>
  bool operator!=(address_slice<other_up, other_low> other) const { return !(*this == other); }

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

  friend class address;
  using underlying_type = address::underlying_type;
  constexpr static int bits = std::numeric_limits<underlying_type>::digits;
  underlying_type value{};

  friend std::ostream& operator<<(std::ostream& stream, const address_slice<dynamic_extent, dynamic_extent> &addr)
  {
    stream << "0x" << std::hex << addr.to<underlying_type>() << std::dec;
    return stream;
  }

  public:
  explicit address_slice(champsim::address val) : address_slice(champsim::address::bits, 0, val) {}
  address_slice(std::size_t up, std::size_t low, champsim::address val) : upper(up), lower(low), value(val.value)
  {
    assert(up >= low);
    assert(up <= bits);
    assert(low <= bits);
  }

  template <typename T>
  T to() const
  {
    //todo check preconditions
    static_assert(std::is_integral_v<T>);
    underlying_type raw_val = (value >> lower) & champsim::bitmask(upper-lower);
    assert(raw_val <= std::numeric_limits<T>::max());
    assert(static_cast<T>(raw_val) >= std::numeric_limits<T>::min());
    return static_cast<T>(raw_val);
  }

  address to_address() const
  {
    return champsim::address{value & champsim::bitmask(upper, lower)};
  }

  template <std::size_t other_up, std::size_t other_low>
  bool operator==(address_slice<other_up, other_low> other) const
  {
    assert(this->upper == other.upper);
    assert(this->lower == other.lower);
    return (value & bitmask(upper, lower)) == (other.value & bitmask(upper, lower));
  }

  template <std::size_t other_up, std::size_t other_low>
  bool operator!=(address_slice<other_up, other_low> other) const { return !(*this == other); }

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
address_slice<UP, LOW> address::slice() const
{
  static_assert(LOW <= bits);
  static_assert(UP <= bits);
  return address_slice<UP, LOW>{*this};
}

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

}

#endif
