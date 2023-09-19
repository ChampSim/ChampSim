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

    /**
     * Default-initialize the slice. This constructor is invalid for dynamically-sized extents.
     */
    constexpr address_slice() : extent{}, value{} {}

    /**
     * Initialize the slice with the given raw value. This constructor is invalid for dynamically-sized extents.
     */
    constexpr explicit address_slice(underlying_type val) : extent{}, value(val) {}

    /**
     * Initialize the slice with the given slice, shifting and masking if necessary. If this extent is dynamic, it will have the same bounds as the given slice.
     */
    template <typename OTHER_EXT>
    constexpr explicit address_slice(const address_slice<OTHER_EXT>& val) : address_slice(maybe_dynamic(val.extent), val) {}

    /**
     * Initialize the slice with the given slice, shifting and masking if necessary. The extent type can be deduced from the first argument.
     */
    template <typename OTHER_EXT>
    constexpr address_slice(extent_type ext, const address_slice<OTHER_EXT>& val) : extent(ext), value(((val.value << val.lower_extent()) & bitmask(ext.upper, ext.lower)) >> ext.lower)
    {
      assert(ext.upper <= bits);
      assert(ext.lower <= bits);
    }

    /**
     * Initialize the slice with the given value. The extent type can be deduced from the first argument.
     */
    constexpr address_slice(extent_type ext, underlying_type val) : extent(ext), value(val & bitmask(ext.upper-ext.lower))
    {
      assert(ext.upper <= bits);
      assert(ext.lower <= bits);
    }

    constexpr static int bits = std::numeric_limits<underlying_type>::digits;

    template <typename Ostr, typename ST>
    friend Ostr& operator<<(Ostr& stream, const address_slice<ST>& addr);

    /**
     * Unwrap the value of this slice as a raw integer value.
     */
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

    /**
     * Compare with another address slice for equality.
     *
     * \throws std::invalid_argument{ If the extents do not match }
     */
    [[nodiscard]] constexpr bool operator==(self_type other) const
    {
      if (upper_extent() != other.upper_extent())
        throw std::invalid_argument{"Upper bounds do not match"};
      if (lower_extent() != other.lower_extent())
        throw std::invalid_argument{"Lower bounds do not match"};
      return value == other.value;
    }

    /**
     * Compare with another address slice for ordering.
     *
     * \throws std::invalid_argument{ If the extents do not match }
     */
    [[nodiscard]] constexpr bool operator< (self_type other) const
    {
      if (upper_extent() != other.upper_extent())
        throw std::invalid_argument{"Upper bounds do not match"};
      if (lower_extent() != other.lower_extent())
        throw std::invalid_argument{"Lower bounds do not match"};
      return value < other.value;
    }

    /**
     * Compare with another address slice for inequality.
     *
     * \throws std::invalid_argument{ If the extents do not match }
     */
    [[nodiscard]] constexpr bool operator!=(self_type other) const { return !(*this == other); }

    /**
     * Compare with another address slice for ordering.
     *
     * \throws std::invalid_argument{ If the extents do not match }
     */
    [[nodiscard]] constexpr bool operator<=(self_type other) const { return *this < other || *this == other; }

    /**
     * Compare with another address slice for ordering.
     *
     * \throws std::invalid_argument{ If the extents do not match }
     */
    [[nodiscard]] constexpr bool operator> (self_type other) const { return !(value <= other.value); }

    /**
     * Compare with another address slice for ordering.
     *
     * \throws std::invalid_argument{ If the extents do not match }
     */
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

    /**
     * Perform a slice on this address. The given extent should be relative to this slice's extent.
     * If either this extent or the subextent are runtime-sized, the result will have a runtime-sized extent.
     * Otherwise, the extent will be statically-sized.
     */
    template <typename SUB_EXTENT>
    [[nodiscard]] auto slice(SUB_EXTENT subextent) const
    {
      auto new_ext = relative_extent(extent, subextent);
      return address_slice<decltype(new_ext)>{new_ext, value >> subextent.lower};
    }

    /**
     * Slice the upper bits, ending with the given bit relative to the lower extent of this.
     * If this slice is statically-sized, the result will be statically-sized.
     */
    template <std::size_t new_lower>
    [[nodiscard]] auto slice_upper() const
    {
      return slice(static_extent<bits, new_lower>{});
    }

    /**
     * Slice the lower bits, ending with the given bit relative to the lower extent of this.
     * If this slice is statically-sized, the result will be statically-sized.
     */
    template <std::size_t new_upper>
    [[nodiscard]] auto slice_lower() const
    {
      return slice(static_extent<new_upper, 0>{});
    }

    /**
     * Slice the upper bits, ending with the given bit relative to the lower extent of this.
     * The result of this will always be runtime-sized.
     */
    [[nodiscard]] auto slice_upper(std::size_t new_lower) const
    {
      return slice(dynamic_extent{bits, new_lower});
    }

    /**
     * Slice the lower bits, ending with the given bit relative to the lower extent of this.
     * The result of this will always be runtime-sized.
     */
    [[nodiscard]] auto slice_lower(std::size_t new_upper) const
    {
      return slice(dynamic_extent{new_upper, 0});
    }

    /**
     * Get the upper portion of the extent.
     */
    [[nodiscard]] constexpr auto upper_extent() const {
      if constexpr (detail::extent_is_static<extent_type>) {
        return extent_type::upper;
      } else {
        return extent.upper;
      }
    }

    /**
     * Get the lower portion of the extent.
     */
    [[nodiscard]] constexpr auto lower_extent() const {
      if constexpr (detail::extent_is_static<extent_type>) {
        return extent_type::lower;
      } else {
        return extent.lower;
      }
    }
};

/**
 * Find the offset between two slices with the same types.
 */
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

/**
 * Join address slices together. Later slices will overwrite bits from earlier slices.
 * The extent of the returned slice is the superset of all slices.
 * If all of the slices are statically-sized, the result will be statically-sized as well.
 */
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
