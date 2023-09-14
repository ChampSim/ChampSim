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

#ifdef CHAMPSIM_MODULE
#define SET_ASIDE_CHAMPSIM_MODULE
#undef CHAMPSIM_MODULE
#endif

#ifndef ADDRESS_H
#define ADDRESS_H

#include <algorithm>
#include <cstdint>
#include <cassert>
#include <charconv>
#include <ios>
#include <iomanip>
#include <limits>
#include <fmt/core.h>
#include <fmt/format.h>

#include "util/bits.h"

namespace champsim {
template <std::size_t UPPER, std::size_t LOWER=UPPER>
class address_slice;
inline constexpr std::size_t dynamic_extent = std::numeric_limits<std::size_t>::max();

template <std::size_t UP, std::size_t LOW>
[[nodiscard]] constexpr auto offset(address_slice<UP, LOW> base, address_slice<UP, LOW> other) -> typename address_slice<UP, LOW>::difference_type;

template <typename... Slices>
[[nodiscard]] constexpr auto splice(Slices... slices);

template <std::size_t UP_A, std::size_t LOW_A, std::size_t UP_B, std::size_t LOW_B, typename... OtherSlices>
[[nodiscard]] constexpr auto splice(address_slice<UP_A, LOW_A> lhs, address_slice<UP_B, LOW_B> rhs, OtherSlices... others);

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

    template <typename Ostr, typename ST>
    friend Ostr& operator<<(Ostr& stream, const address_slice_impl<ST>& addr);

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
      value += static_cast<underlying_type>(delta);
      value &= bitmask(derived.upper - derived.lower);
      return derived;
    }

    [[nodiscard]] constexpr self_type operator+(difference_type delta) const
    {
      self_type retval = static_cast<const self_type&>(*this);
      retval += delta;
      return retval;
    }

    constexpr self_type& operator-=(difference_type delta) { return operator+=(-delta); }
    [[nodiscard]] constexpr self_type operator-(difference_type delta) const { return operator+(-delta); }

    constexpr self_type& operator++() { return operator+=(1); }
    constexpr self_type  operator++(int) {
      self_type retval = static_cast<const self_type&>(*this);
      operator++();
      return retval;
    }

    constexpr self_type& operator--() { return operator-=(1); }
    constexpr self_type  operator--(int) {
      self_type retval = static_cast<const self_type&>(*this);
      operator--();
      return retval;
    }

    template <std::size_t slice_upper, std::size_t slice_lower, typename D = self_type, std::enable_if_t<D::is_static, bool> = true>
    [[nodiscard]] constexpr auto slice() const -> address_slice<D::lower + slice_upper, D::lower + slice_lower>
    {
      const self_type& derived = static_cast<const self_type&>(*this);
      static_assert(slice_lower <= (D::upper - D::lower));
      static_assert(slice_upper <= (D::upper - D::lower));
      return address_slice<D::lower + slice_upper, D::lower + slice_lower>{derived};
    }

    template <std::size_t new_lower, typename D = self_type, std::enable_if_t<D::is_static, bool> = true>
    [[nodiscard]] constexpr auto slice_upper() const -> address_slice<D::upper - D::lower, new_lower>
    {
      return slice<D::upper - D::lower, new_lower, D>();
    }

    template <std::size_t new_upper, typename D = self_type, std::enable_if_t<D::is_static, bool> = true>
    [[nodiscard]] constexpr auto slice_lower() const -> address_slice<new_upper, 0>
    {
      return slice<new_upper, 0, D>();
    }

    [[nodiscard]] constexpr address_slice<dynamic_extent, dynamic_extent> slice(std::size_t slice_upper, std::size_t slice_lower) const;
    [[nodiscard]] constexpr address_slice<dynamic_extent, dynamic_extent> slice_lower(std::size_t new_upper) const;
    [[nodiscard]] constexpr address_slice<dynamic_extent, dynamic_extent> slice_upper(std::size_t new_lower) const;
};
}

template <>
class address_slice<dynamic_extent, dynamic_extent> : public detail::address_slice_impl<address_slice<dynamic_extent, dynamic_extent>>
{
  using self_type = address_slice<dynamic_extent, dynamic_extent>;
  using impl_type = detail::address_slice_impl<self_type>;

  constexpr static bool is_static = false;

  std::size_t upper{impl_type::bits};
  std::size_t lower{0};

  template <std::size_t U, std::size_t L> friend class address_slice;
  friend impl_type;

  template <std::size_t UP_A, std::size_t LOW_A, std::size_t UP_B, std::size_t LOW_B, typename... OtherSlices>
    friend constexpr auto splice(address_slice<UP_A, LOW_A> lhs, address_slice<UP_B, LOW_B> rhs, OtherSlices... others);

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

  constexpr static bool is_static = true;
  constexpr static std::size_t upper{UP};
  constexpr static std::size_t lower{LOW};

  template <std::size_t U, std::size_t L> friend class address_slice;
  friend impl_type;

  static_assert(UP != LOW, "An address slice of zero width is probably a bug");
  static_assert(LOW <= UP);
  static_assert(UP <= impl_type::bits);
  static_assert(LOW <= impl_type::bits);

  template <std::size_t UP_A, std::size_t LOW_A, std::size_t UP_B, std::size_t LOW_B, typename... OtherSlices>
    friend constexpr auto splice(address_slice<UP_A, LOW_A> lhs, address_slice<UP_B, LOW_B> rhs, OtherSlices... others);

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

template <std::size_t UP_A, std::size_t LOW_A, std::size_t UP_B, std::size_t LOW_B, typename... OtherSlices>
constexpr auto splice(address_slice<UP_A, LOW_A> lhs, address_slice<UP_B, LOW_B> rhs, OtherSlices... others)
{
  if constexpr (sizeof...(OtherSlices) == 0) {
    if constexpr (decltype(lhs)::is_static && decltype(rhs)::is_static) {
      constexpr auto upper_extent{std::max<std::size_t>(lhs.upper, rhs.upper)};
      constexpr auto lower_extent{std::min<std::size_t>(lhs.lower, rhs.lower)};
      using rettype = address_slice<upper_extent, lower_extent>;
      return rettype{splice_bits(rettype{lhs}.value, rettype{rhs}.value, rhs.upper - lower_extent, rhs.lower - lower_extent)};
    } else {
      const auto upper_extent{std::max<std::size_t>(lhs.upper, rhs.upper)};
      const auto lower_extent{std::min<std::size_t>(lhs.lower, rhs.lower)};
      using rettype = address_slice<dynamic_extent, dynamic_extent>;
      return rettype{upper_extent, lower_extent, splice_bits(rettype{upper_extent, lower_extent, lhs}.value, rettype{upper_extent, lower_extent, rhs}.value, rhs.upper - lower_extent, rhs.lower - lower_extent)};
    }
  } else {
    return splice(lhs, splice(rhs, others...));
  }
}

namespace detail {
template <typename Ostr, typename self_type>
Ostr& operator<<(Ostr& stream, const champsim::detail::address_slice_impl<self_type> &addr)
{
  auto addr_flags = std::ios_base::hex | std::ios_base::showbase | std::ios_base::internal;
  auto addr_mask  = std::ios_base::basefield | std::ios_base::showbase | std::ios_base::adjustfield;

  auto oldflags = stream.setf(addr_flags, addr_mask);
  auto oldfill = stream.fill('0');

  stream << addr.template to<typename champsim::detail::address_slice_impl<self_type>::underlying_type>();

  stream.setf(oldflags, addr_mask);
  stream.fill(oldfill);

  return stream;
}
}
}

template <std::size_t UP, std::size_t LOW>
struct fmt::formatter<champsim::address_slice<UP, LOW>>
  //: fmt::formatter<typename champsim::address_slice<UP, LOW>::underlying_type>
{
  using addr_type = champsim::address_slice<UP, LOW>;
  //using underlying_type = typename addr_type::underlying_type;
  std::size_t len = std::numeric_limits<std::size_t>::max();

  constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator
  {
    auto [ret_ptr, ec] = std::from_chars(ctx.begin(), ctx.end(), len);
    // Check if reached the end of the range:
    if (ec == std::errc::result_out_of_range || (ret_ptr != ctx.end() && *ret_ptr != '}')) fmt::throw_format_error("invalid format");
    return ret_ptr;
  }

  auto format(const addr_type& addr, fmt::format_context& ctx) const -> fmt::format_context::iterator
  {
    //return fmt::formatter<underlying_type>::format(addr.template to<underlying_type>(), ctx);
    if (len == std::numeric_limits<std::size_t>::max())
      return fmt::format_to(ctx.out(), "{:#0x}", addr.template to<typename addr_type::underlying_type>());
    return fmt::format_to(ctx.out(), "{:#0{}x}", addr.template to<typename addr_type::underlying_type>(), len);
  }
};

#endif

#ifdef SET_ASIDE_CHAMPSIM_MODULE
#undef SET_ASIDE_CHAMPSIM_MODULE
#define CHAMPSIM_MODULE
#endif
