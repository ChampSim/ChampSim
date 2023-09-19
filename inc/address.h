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
#include <numeric>
#include <type_traits>
#include <fmt/core.h>
#include <fmt/format.h>

#include "extent.h"
#include "util/bits.h"

namespace champsim {
template <typename Extent>
class address_slice;

template <typename Extent>
[[nodiscard]] constexpr auto offset(address_slice<Extent> base, address_slice<Extent> other) -> typename address_slice<Extent>::difference_type;

template <typename... Extents>
[[nodiscard]] constexpr auto splice(address_slice<Extents>... slices);

namespace detail
{
  template <typename Extent>
  struct splice_fold_wrapper;
}

template <typename EXTENT>
class address_slice
{
  using extent_type = EXTENT;
  using self_type = address_slice<extent_type>;

  extent_type extent;

  template <typename> friend class address_slice;
  friend class detail::splice_fold_wrapper<extent_type>;

  public:
    using underlying_type = uint64_t;
    using difference_type = std::make_signed_t<underlying_type>;

  private:
    underlying_type value{};

    template <typename OTHER_EXT>
    static extent_type maybe_dynamic(OTHER_EXT other)
    {
      if constexpr (detail::extent_is_static<extent_type>) {
        (void)other;
        return extent_type{};
      } else {
        return extent_type{other.upper, other.lower};
      }
    }

  public:
    template <typename E>
      friend constexpr auto offset(address_slice<E> base, address_slice<E> other) -> typename address_slice<E>::difference_type;

    constexpr address_slice() : extent{}, value{} {}
    constexpr explicit address_slice(underlying_type val) : extent{}, value(val) {}

    template <typename OTHER_EXT>
    constexpr explicit address_slice(const address_slice<OTHER_EXT>& val) : address_slice(maybe_dynamic(val.extent), val) {}

    template <typename OTHER_EXT>
    constexpr address_slice(extent_type ext, const address_slice<OTHER_EXT>& val) : extent(ext), value(((val.value << val.lower_extent()) & bitmask(ext.upper, ext.lower)) >> ext.lower)
    {
      assert(ext.upper <= bits);
      assert(ext.lower <= bits);
    }

    constexpr address_slice(extent_type ext, underlying_type val) : extent(ext), value(val & bitmask(ext.upper-ext.lower))
    {
      assert(ext.upper <= bits);
      assert(ext.lower <= bits);
    }

    constexpr static int bits = std::numeric_limits<underlying_type>::digits;

    template <typename Ostr, typename ST>
    friend Ostr& operator<<(Ostr& stream, const address_slice<ST>& addr);

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
      if (upper_extent() != other.upper_extent())
        throw std::invalid_argument{"Upper bounds do not match"};
      if (lower_extent() != other.lower_extent())
        throw std::invalid_argument{"Lower bounds do not match"};
      return value == other.value;
    }

    [[nodiscard]] constexpr bool operator< (self_type other) const
    {
      if (upper_extent() != other.upper_extent())
        throw std::invalid_argument{"Upper bounds do not match"};
      if (lower_extent() != other.lower_extent())
        throw std::invalid_argument{"Lower bounds do not match"};
      return value < other.value;
    }

    [[nodiscard]] constexpr bool operator!=(self_type other) const { return !(*this == other); }
    [[nodiscard]] constexpr bool operator<=(self_type other) const { return *this < other || *this == other; }
    [[nodiscard]] constexpr bool operator> (self_type other) const { return !(value <= other.value); }
    [[nodiscard]] constexpr bool operator>=(self_type other) const { return *this > other || *this == other; }

    constexpr self_type& operator+=(difference_type delta)
    {
      value += static_cast<underlying_type>(delta);
      value &= bitmask(upper_extent() - lower_extent());
      return *this;
    }

    [[nodiscard]] constexpr self_type operator+(difference_type delta) const
    {
      self_type retval = *this;
      retval += delta;
      return retval;
    }

    constexpr self_type& operator-=(difference_type delta) { return operator+=(-delta); }
    [[nodiscard]] constexpr self_type operator-(difference_type delta) const { return operator+(-delta); }

    constexpr self_type& operator++() { return operator+=(1); }
    constexpr self_type  operator++(int) {
      self_type retval = *this;
      operator++();
      return retval;
    }

    constexpr self_type& operator--() { return operator-=(1); }
    constexpr self_type  operator--(int) {
      self_type retval = *this;
      operator--();
      return retval;
    }

    template <typename SUB_EXTENT>
    [[nodiscard]] auto slice(SUB_EXTENT subextent) const
    {
      assert(subextent.lower <= (upper_extent() - lower_extent()));
      assert(subextent.upper <= (upper_extent() - lower_extent()));

      if constexpr (detail::extent_is_static<extent_type> && detail::extent_is_static<SUB_EXTENT>) {
        using return_extent_type = static_extent<extent_type::lower + SUB_EXTENT::upper, extent_type::lower + SUB_EXTENT::lower>;
        return address_slice<return_extent_type>(return_extent_type{}, value >> subextent.lower);
      } else {
        using return_extent_type = dynamic_extent;
        return_extent_type ext{lower_extent() + subextent.upper, lower_extent() + subextent.lower};
        return address_slice<return_extent_type>(ext, value >> subextent.lower);
      }
    }

    template <std::size_t new_lower>
    [[nodiscard]] auto slice_upper() const
    {
      if constexpr (detail::extent_is_static<extent_type>) {
        static_extent<extent_type::upper - extent_type::lower, new_lower> ext{};
        return slice(ext);
      } else {
        return slice(dynamic_extent{upper_extent() - lower_extent(), new_lower});
      }
    }

    template <std::size_t new_upper>
    [[nodiscard]] auto slice_lower() const
    {
      if constexpr (detail::extent_is_static<extent_type>) {
        return slice(static_extent<new_upper, 0>{});
      } else {
        return slice(dynamic_extent{new_upper, 0});
      }
    }

    [[nodiscard]] auto slice_upper(std::size_t new_lower) const
    {
      return slice(dynamic_extent{upper_extent() - lower_extent(), new_lower});
    }

    [[nodiscard]] auto slice_lower(std::size_t new_upper) const
    {
      return slice(dynamic_extent{new_upper, 0});
    }

    [[nodiscard]] constexpr auto upper_extent() const {
      if constexpr (detail::extent_is_static<extent_type>) {
        return extent_type::upper;
      } else {
        return extent.upper;
      }
    }

    [[nodiscard]] constexpr auto lower_extent() const {
      if constexpr (detail::extent_is_static<extent_type>) {
        return extent_type::lower;
      } else {
        return extent.lower;
      }
    }
};

template <typename Extent>
constexpr auto offset(address_slice<Extent> base, address_slice<Extent> other) -> typename address_slice<Extent>::difference_type
{
  using underlying_type = typename address_slice<Extent>::underlying_type;
  using difference_type = typename address_slice<Extent>::difference_type;

  underlying_type abs_diff = (base.value > other.value) ? (base.value - other.value) : (other.value - base.value);
  assert(abs_diff <= std::numeric_limits<difference_type>::max());
  return (base.value > other.value) ? -static_cast<difference_type>(abs_diff) : static_cast<difference_type>(abs_diff);
}

namespace detail
{
  template <typename Extent>
  struct splice_fold_wrapper
  {
    using extent_type = Extent;
    using value_type = typename address_slice<extent_type>::underlying_type;

    extent_type extent;
    value_type underlying;

    explicit splice_fold_wrapper(address_slice<extent_type> addr) : splice_fold_wrapper(addr.extent, addr.value) {}
    splice_fold_wrapper(extent_type ext, value_type und) : extent(ext), underlying(und) {}

    template <typename OtherExtent>
    auto operator+(splice_fold_wrapper<OtherExtent> other) const {
        auto return_extent = extent_union(this->extent, other.extent);
        return splice_fold_wrapper<decltype(return_extent)>{
            return_extent,
            splice_bits(
                underlying << (extent.lower - return_extent.lower),
                other.underlying << (other.extent.lower - return_extent.lower),
                other.extent.upper - return_extent.lower, other.extent.lower - return_extent.lower
            )
        };
    }

    auto address() const
    {
      return address_slice{extent, underlying};
    }
  };
}

template <typename... Extents>
constexpr auto splice(address_slice<Extents>... slices)
{
  return (detail::splice_fold_wrapper<Extents>{slices} + ...).address();
}

template <typename Ostr, typename extent_type>
Ostr& operator<<(Ostr& stream, const champsim::address_slice<extent_type> &addr)
{
  auto addr_flags = std::ios_base::hex | std::ios_base::showbase | std::ios_base::internal;
  auto addr_mask  = std::ios_base::basefield | std::ios_base::showbase | std::ios_base::adjustfield;

  auto oldflags = stream.setf(addr_flags, addr_mask);
  auto oldfill = stream.fill('0');

  stream << addr.template to<typename champsim::address_slice<extent_type>::underlying_type>();

  stream.setf(oldflags, addr_mask);
  stream.fill(oldfill);

  return stream;
}
}

template <typename Extent>
struct fmt::formatter<champsim::address_slice<Extent>>
  //: fmt::formatter<typename champsim::address_slice<UP, LOW>::underlying_type>
{
  using addr_type = champsim::address_slice<Extent>;
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
