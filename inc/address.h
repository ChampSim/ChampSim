#ifndef ADDRESS_H
#define ADDRESS_H

#include <algorithm>
#include <cassert>
#include <ios>
#include <iostream>

#include "champsim_constants.h"
#include "util/bits.h"

namespace champsim {
class address
{
  public:
  using underlying_type = uint64_t;
  using difference_type = int64_t;

  private:
  underlying_type value{};

  friend std::ostream& operator<<(std::ostream& stream, const champsim::address &addr)
  {
    stream << "0x" << std::hex << addr.value << std::dec;
    return stream;
  }

  class internal_slice
  {
    std::size_t upper{};
    std::size_t lower{};
    underlying_type value{};

    friend class address;

    friend std::ostream& operator<<(std::ostream& stream, const champsim::address::internal_slice &addr)
    {
      stream << "0x" << std::hex << addr.to<underlying_type>() << std::dec;
      return stream;
    }

    constexpr internal_slice(std::size_t up, std::size_t low, underlying_type val) : upper(up), lower(low), value(val) {}

    public:
    template <typename T>
    T to() const
    {
      //todo check preconditions
      static_assert(std::is_integral_v<T>);
      underlying_type raw_val = (value >> lower) & champsim::bitmask(upper-lower);
      assert(raw_val <= std::numeric_limits<T>::max());
      assert(raw_val >= std::numeric_limits<T>::min());
      return static_cast<T>(raw_val);
    }

    constexpr address to_address() const
    {
      return champsim::address{value & champsim::bitmask(upper, lower)};
    }

    constexpr bool operator==(internal_slice other) const
    {
      assert(this->upper == other.upper);
      assert(this->lower == other.lower);
      return (value & bitmask(upper, lower)) == (other.value & bitmask(upper, lower));
    }
    constexpr bool operator!=(internal_slice other) const { return !(*this == other); }

    //internal_slice operator|(internal_slice other)
    //{
      //internal_slice retval;
      //retval.upper = std::min(this->upper, other.upper);
      //retval.lower = std::min(this->lower, other.lower);
      //retval.value = this->value | other.value;
      //return retval;
    //}
  };

  public:
  constexpr static std::size_t ALL_UPPER = 8*sizeof(address::underlying_type);
  constexpr static std::size_t ALL_LOWER = 0;

  constexpr address() = default;
  constexpr explicit address(underlying_type addr) : value(addr) {}

  constexpr internal_slice slice(std::size_t upper, std::size_t lower) const { return internal_slice{upper, lower, value}; }
  constexpr internal_slice slice_upper(std::size_t lower) const { return slice(ALL_UPPER, lower); }
  constexpr internal_slice slice_lower(std::size_t upper) const { return slice(upper, ALL_LOWER); }
  constexpr auto block_address() const { return slice_upper(LOG2_BLOCK_SIZE); }
  constexpr auto page_address() const { return slice_upper(LOG2_PAGE_SIZE); }

  constexpr bool is_block_address() const { return *this == block_address().to_address(); }
  constexpr bool is_page_address() const { return *this == page_address().to_address(); }

  constexpr static difference_type offset(address base, address other) {
    underlying_type abs_diff = (base.value > other.value) ? (base.value - other.value) : (other.value - base.value);
    assert(abs_diff <= std::numeric_limits<difference_type>::max());
    return (base.value > other.value) ? -static_cast<difference_type>(abs_diff) : static_cast<difference_type>(abs_diff);
  }

  constexpr static address splice(address upper, address lower, std::size_t bits) { return address{champsim::splice_bits(upper.value, lower.value, bits)}; }

  constexpr bool operator==(address other) const { return value == other.value; }
  constexpr bool operator!=(address other) const { return !(*this == other); }
  constexpr bool operator< (address other) const { return value < other.value; }
  constexpr bool operator<=(address other) const { return *this < other || *this == other; }
  constexpr bool operator> (address other) const { return !(value <= other.value); }
  constexpr bool operator>=(address other) const { return *this > other || *this == other; }

  constexpr address& operator+=(difference_type delta) { value += delta; return *this; }
  constexpr address operator+(difference_type delta) { address retval{*this}; retval += delta; return retval; }
  constexpr address& operator-=(difference_type delta) { value -= delta; return *this; }
  constexpr address operator-(difference_type delta) { address retval{*this}; retval -= delta; return retval; }
};
}

#endif
