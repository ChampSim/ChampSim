#ifndef ADDRESS_H
#define ADDRESS_H

#include <algorithm>
#include <cassert>
#include <ios>
#include <iostream>

#include "util/bits.h"
#include "util/detect.h"

namespace champsim {
template <std::size_t UPPER, std::size_t LOWER=UPPER>
class address_slice;
inline constexpr std::size_t dynamic_extent = std::numeric_limits<std::size_t>::max();

template <std::size_t UP, std::size_t LOW>
[[nodiscard]] constexpr auto offset(address_slice<UP, LOW> base, address_slice<UP, LOW> other) -> typename address_slice<UP, LOW>::difference_type;

template <std::size_t UP, std::size_t LOW>
[[nodiscard]] constexpr auto splice(address_slice<UP, LOW> upper, address_slice<UP, LOW> lower, std::size_t bits) -> address_slice<UP, LOW>;

template <std::size_t UP, std::size_t LOW>
[[nodiscard]] constexpr auto splice(address_slice<UP, LOW> upper, address_slice<UP, LOW> lower, std::size_t bits_up, std::size_t bits_low) -> address_slice<UP, LOW>;

template <std::size_t UP_A, std::size_t LOW_A, std::size_t UP_B, std::size_t LOW_B>
[[nodiscard]] constexpr auto splice(address_slice<UP_A, LOW_A> upper, address_slice<UP_B, LOW_B> lower) -> address_slice<std::max(UP_A, UP_B), std::min(LOW_A, LOW_B)>;

namespace detail {
template <typename self_type>
class address_slice_impl
{
  public:
    using underlying_type = uint64_t;
    using difference_type = int64_t;

    underlying_type value{};

    constexpr address_slice_impl() = default; // TODO remove this
    constexpr explicit address_slice_impl(underlying_type val) : value(val) {}

    constexpr static int bits = std::numeric_limits<underlying_type>::digits;

    friend std::ostream& operator<<(std::ostream& stream, const self_type &addr)
    {
      stream << "0x" << std::hex << addr.template to<underlying_type>() << std::dec;
      return stream;
    }

    template <typename T>
    [[nodiscard]] constexpr T to() const
    {
      static_assert(std::is_integral_v<T>);
      if (value > std::numeric_limits<T>::max())
        throw std::domain_error{"Contained value overflows the target type"};
      if (static_cast<T>(value) < std::numeric_limits<T>::min())
        throw std::domain_error{"Contained value underflows the target type"};
      return static_cast<T>(value);
    }

    [[nodiscard]] constexpr bool operator==(self_type other) const
    {
      const self_type& derived = static_cast<const self_type&>(*this);
      if (derived.upper != other.upper)
        throw std::invalid_argument{"Upper bounds do not match"};
      if (derived.lower != other.lower)
        throw std::invalid_argument{"Lower bounds do not match"};
      return value == other.value;
    }

    [[nodiscard]] constexpr bool operator< (self_type other) const
    {
      const self_type& derived = static_cast<const self_type&>(*this);
      if (derived.upper != other.upper)
        throw std::invalid_argument{"Upper bounds do not match"};
      if (derived.lower != other.lower)
        throw std::invalid_argument{"Lower bounds do not match"};
      return value < other.value;
    }

    [[nodiscard]] constexpr bool operator!=(self_type other) const { return !(*this == other); }
    [[nodiscard]] constexpr bool operator<=(self_type other) const { return *this < other || *this == other; }
    [[nodiscard]] constexpr bool operator> (self_type other) const { return !(value <= other.value); }
    [[nodiscard]] constexpr bool operator>=(self_type other) const { return *this > other || *this == other; }

    constexpr self_type& operator+=(difference_type delta)
    {
      self_type& derived = static_cast<self_type&>(*this);
      value = (value + delta) & bitmask(derived.upper - derived.lower);
      return derived;
    }

    constexpr self_type& operator-=(difference_type delta) { return *this += (-delta); }

    [[nodiscard]] constexpr self_type operator+(difference_type delta) const
    {
      self_type retval = static_cast<const self_type&>(*this);
      retval += delta;
      return retval;
    }

    [[nodiscard]] constexpr self_type operator-(difference_type delta) const
    {
      self_type retval = static_cast<const self_type&>(*this);
      retval -= delta;
      return retval;
    }

    // recognizes types that support static slicing
    template <typename T>
    using static_upper = typename decltype( std::declval<T>().upper )::type;
    template <typename T>
    using static_lower = typename decltype( std::declval<T>().lower )::type;

    template <std::size_t slice_upper, std::size_t slice_lower, typename D = self_type>
    [[nodiscard]] constexpr auto slice() const -> address_slice<static_lower<D>::value + slice_upper, static_lower<D>::value + slice_lower>
    {
      const self_type& derived = static_cast<const self_type&>(*this);
      static_assert(slice_lower <= (static_upper<D>::value - static_lower<D>::value));
      static_assert(slice_upper <= (static_upper<D>::value - static_lower<D>::value));
      return address_slice<static_lower<D>::value + slice_upper, static_lower<D>::value + slice_lower>{derived};
    }

    template <std::size_t new_lower, typename D = self_type>
    [[nodiscard]] constexpr auto slice_upper() const -> address_slice<static_upper<D>::value - static_lower<D>::value, new_lower>
    {
      return slice<static_upper<D>::value - static_lower<D>::value, new_lower, D>();
    }

    template <std::size_t new_upper, typename D = self_type>
    [[nodiscard]] constexpr auto slice_lower() const -> address_slice<new_upper, 0>
    {
      return slice<new_upper, 0, D>();
    }

    [[nodiscard]] constexpr address_slice<dynamic_extent, dynamic_extent> slice(std::size_t slice_upper, std::size_t slice_lower) const;
    [[nodiscard]] constexpr address_slice<dynamic_extent, dynamic_extent> slice_lower(std::size_t new_upper) const;
    [[nodiscard]] constexpr address_slice<dynamic_extent, dynamic_extent> slice_upper(std::size_t new_lower) const;
};

template <typename LHS, typename RHS>
struct splice_helper
{
  using left_type = LHS;
  using right_type = RHS;

  template <typename T>
    using static_upper = typename decltype( std::declval<T>().upper )::type;
  template <typename T>
    using static_lower = typename decltype( std::declval<T>().lower )::type;

  constexpr static bool left_is_static = champsim::is_detected_v<static_upper, left_type>;
  constexpr static bool right_is_static = champsim::is_detected_v<static_upper, right_type>;
  using return_type = std::conditional<
    !left_is_static || !right_is_static,
    address_slice<dynamic_extent, dynamic_extent>,
    address_slice<std::max(static_upper<left_type>::value, static_upper<right_type>::value), std::min(static_lower<left_type>::value, static_lower<right_type>::value)>
  >;

  [[nodiscard]] constexpr static auto splice(left_type lhs, right_type rhs) -> typename return_type::type
  {
    using rettype = typename return_type::type;
    if constexpr (left_is_static && right_is_static) {
      return rettype{splice_bits(rettype{lhs}.value, rettype{rhs}.value, rhs.upper - std::min<std::size_t>(lhs.lower, rhs.lower), rhs.lower - std::min<std::size_t>(lhs.lower, rhs.lower))};
    } else {
      const auto upper = std::max(lhs.upper, rhs.upper);
      const auto lower = std::min(lhs.lower, rhs.lower);
      return rettype{upper, lower, splice_bits(rettype{upper, lower, lhs}.value, rettype{upper, lower, rhs}.value, rhs.upper - lower, rhs.lower - lower)};
    }
  }
};
}

template <>
class address_slice<dynamic_extent, dynamic_extent> : public detail::address_slice_impl<address_slice<dynamic_extent, dynamic_extent>>
{
  using self_type = address_slice<dynamic_extent, dynamic_extent>;
  using impl_type = detail::address_slice_impl<self_type>;

  std::size_t upper{impl_type::bits};
  std::size_t lower{0};

  template <std::size_t U, std::size_t L> friend class address_slice;
  friend impl_type;
  template <std::size_t U, std::size_t L> friend class detail::splice_helper;

  template <std::size_t U, std::size_t L>
    friend constexpr auto splice(address_slice<U, L> upper, address_slice<U, L> lower, std::size_t bits) -> address_slice<U, L>;

  template <std::size_t UP_A, std::size_t LOW_A, std::size_t UP_B, std::size_t LOW_B>
    friend constexpr auto splice(address_slice<UP_A, LOW_A> upper, address_slice<UP_B, LOW_B> lower) -> address_slice<std::max(UP_A, UP_B), std::min(LOW_A, LOW_B)>;

  public:
  using typename impl_type::underlying_type;
  using typename impl_type::difference_type;

  constexpr address_slice() = default;

  constexpr explicit address_slice(underlying_type val) : address_slice(impl_type::bits, 0, val) {}

  template <std::size_t other_up, std::size_t other_low,
           typename test_up = std::enable_if_t<other_up != dynamic_extent, void>,
           typename test_low = std::enable_if_t<other_low != dynamic_extent, void>>
  constexpr explicit address_slice(address_slice<other_up, other_low> val) : address_slice(other_up, other_low, val) {}

  template <std::size_t other_up, std::size_t other_low>
  constexpr address_slice(std::size_t up, std::size_t low, address_slice<other_up, other_low> val) : impl_type(((val.value << val.lower) & bitmask(up, low)) >> low), upper(up), lower(low)
  {
    assert(up >= low);
    assert(up <= impl_type::bits);
    assert(low <= impl_type::bits);
  }

  constexpr address_slice(std::size_t up, std::size_t low, underlying_type val) : impl_type(val & bitmask(up-low)), upper(up), lower(low)
  {
    assert(up >= low);
    assert(up <= impl_type::bits);
    assert(low <= impl_type::bits);
  }

  [[nodiscard]] constexpr std::size_t upper_extent() const { return upper; }
  [[nodiscard]] constexpr std::size_t lower_extent() const { return lower; }
};

template <std::size_t UP, std::size_t LOW>
class address_slice : public detail::address_slice_impl<address_slice<UP, LOW>>
{
  using self_type = address_slice<UP, LOW>;
  using impl_type = detail::address_slice_impl<self_type>;

  constexpr static std::integral_constant<std::size_t, UP> upper{};
  constexpr static std::integral_constant<std::size_t, LOW> lower{};

  template <std::size_t U, std::size_t L> friend class address_slice;
  friend impl_type;
  template <std::size_t U, std::size_t L> friend class detail::splice_helper;

  static_assert(UP != LOW, "An address slice of zero width is probably a bug");
  static_assert(LOW <= UP);
  static_assert(UP <= impl_type::bits);
  static_assert(LOW <= impl_type::bits);

  template <std::size_t U, std::size_t L>
    friend constexpr auto splice(address_slice<U, L> upper, address_slice<U, L> lower, std::size_t bits) -> address_slice<U, L>;

  template <std::size_t UP_A, std::size_t LOW_A, std::size_t UP_B, std::size_t LOW_B>
    friend constexpr auto splice(address_slice<UP_A, LOW_A> upper, address_slice<UP_B, LOW_B> lower) -> address_slice<std::max(UP_A, UP_B), std::min(LOW_A, LOW_B)>;

  public:
  using typename impl_type::underlying_type;
  using typename impl_type::difference_type;

  constexpr address_slice() = default;

  constexpr explicit address_slice(underlying_type val) : impl_type(val & bitmask(upper-lower)) {}

  template <std::size_t other_up, std::size_t other_low>
  constexpr explicit address_slice(address_slice<other_up, other_low> val) : address_slice(((val.value << val.lower) & bitmask(upper, lower)) >> lower) {}

  [[nodiscard]] constexpr static std::size_t upper_extent() { return upper; }
  [[nodiscard]] constexpr static std::size_t lower_extent() { return lower; }
};

template <std::size_t UP, std::size_t LOW>
address_slice(std::size_t, std::size_t, address_slice<UP, LOW>) -> address_slice<dynamic_extent, dynamic_extent>;

address_slice(std::size_t, std::size_t, detail::address_slice_impl<address_slice<dynamic_extent, dynamic_extent>>::underlying_type) -> address_slice<dynamic_extent, dynamic_extent>;

template <typename self_type>
constexpr auto detail::address_slice_impl<self_type>::slice(std::size_t slice_upper, std::size_t slice_lower) const -> address_slice<dynamic_extent, dynamic_extent>
{
  const self_type& derived = static_cast<const self_type&>(*this);
  assert(slice_lower <= (derived.upper - derived.lower));
  assert(slice_upper <= (derived.upper - derived.lower));
  return address_slice<dynamic_extent, dynamic_extent>{slice_upper + derived.lower, slice_lower + derived.lower, derived};
}

template <typename self_type>
constexpr auto detail::address_slice_impl<self_type>::slice_lower(std::size_t new_upper) const -> address_slice<dynamic_extent, dynamic_extent>
{
  return slice(new_upper, 0);
}

template <typename self_type>
constexpr auto detail::address_slice_impl<self_type>::slice_upper(std::size_t new_lower) const -> address_slice<dynamic_extent, dynamic_extent>
{
  const self_type& derived = static_cast<const self_type&>(*this);
  return slice(derived.upper-derived.lower, new_lower);
}

template <std::size_t UP, std::size_t LOW>
constexpr auto offset(address_slice<UP, LOW> base, address_slice<UP, LOW> other) -> typename address_slice<UP, LOW>::difference_type
{
  using underlying_type = typename address_slice<UP, LOW>::underlying_type;
  using difference_type = typename address_slice<UP, LOW>::difference_type;

  underlying_type abs_diff = (base.value > other.value) ? (base.value - other.value) : (other.value - base.value);
  assert(abs_diff <= std::numeric_limits<difference_type>::max());
  return (base.value > other.value) ? -static_cast<difference_type>(abs_diff) : static_cast<difference_type>(abs_diff);
}

template <std::size_t UP, std::size_t LOW>
constexpr auto splice(address_slice<UP, LOW> upper, address_slice<UP, LOW> lower, std::size_t bits) -> address_slice<UP, LOW>
{
  return splice(upper, lower, bits, 0);
}

template <std::size_t UP, std::size_t LOW>
constexpr auto splice(address_slice<UP, LOW> upper, address_slice<UP, LOW> lower, std::size_t bits_up, std::size_t bits_low) -> address_slice<UP, LOW>
{
  return address_slice<UP, LOW>{splice_bits(upper.value, lower.value, bits_up, bits_low)};
}

template <std::size_t UP_A, std::size_t LOW_A, std::size_t UP_B, std::size_t LOW_B>
constexpr auto splice(address_slice<UP_A, LOW_A> upper, address_slice<UP_B, LOW_B> lower) -> address_slice<std::max(UP_A, UP_B), std::min(LOW_A, LOW_B)>
{
  return detail::splice_helper<decltype(upper), decltype(lower)>::splice(upper, lower);
}
}

#endif
