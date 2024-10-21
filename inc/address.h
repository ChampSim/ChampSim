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

/** \file */

#ifdef CHAMPSIM_MODULE
#define SET_ASIDE_CHAMPSIM_MODULE
#undef CHAMPSIM_MODULE
#endif

#ifndef ADDRESS_H
#define ADDRESS_H

#include <cassert>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <ios>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <fmt/core.h>
#include <fmt/format.h>

#include "champsim.h"
#include "extent.h"
#include "util/bits.h"
#include "util/units.h"

namespace champsim
{
template <typename Extent>
class address_slice;

/**
 * Find the offset between two slices with the same types.
 *
 * \throws overflow_error if the difference cannot be represented in the difference type
 */
template <typename Extent>
[[nodiscard]] constexpr auto offset(address_slice<Extent> base, address_slice<Extent> other) -> typename address_slice<Extent>::difference_type;

/**
 * Find the offset between two slices with the same types, where the first element must be less than or equal to than the second.
 * The return type of this function is unsigned.
 *
 * \throws overflow_error if the difference cannot be represented in the difference type
 */
template <typename Extent>
[[nodiscard]] constexpr auto uoffset(address_slice<Extent> base, address_slice<Extent> other)
    -> std::make_unsigned_t<typename address_slice<Extent>::difference_type>;

/**
 * Join address slices together. Later slices will overwrite bits from earlier slices.
 * The extent of the returned slice is the superset of all slices.
 * If all of the slices are statically-sized, the result will be statically-sized as well.
 */
template <typename... Extents>
[[nodiscard]] constexpr auto splice(address_slice<Extents>... slices);

namespace detail
{
template <typename Extent>
struct splice_fold_wrapper;
}

/**
 * \class address_slice address.h inc/address.h
 *
 * The correctness of address operations is critical to the correctness of ChampSim.
 *
 * This class is a strong type for addresses to prevent bugs that would result from unchecked operations on raw integers.
 * The primary benefit is this: Most address operation bugs are compiler errors.
 * Those that are not are usually runtime exceptions.
 * If you need to manipulate the bits of an address, you can, but you must explicitly enter an unsafe mode.
 *
 * This class is a generalization of a subset of address bits.
 * Implicit conversions between address slices of different extents are compile-time errors, providing a measure of safety.
 * New slices must be explicitly constructed.
 *
 * \code{.cpp}
 *
 *    // LOG2_PAGE_SIZE = 12
 *    // LOG2_BLOCK_SIZE = 6
 *    address full_addr{0xffff'ffff};
 *    block_number block{full_addr}; // 0xffff'ffc0
 *    page_number page{full_addr}; // 0xffff'f000
 *
 * \endcode
 *
 * All address slices can take their extent as a constructor parameter.
 *
 * \code{.cpp}
 *
 *    address_slice st_full_addr{static_extent<64_b,0_b>{},0xffff'ffff};
 *    address_slice st_block{static_extent<64_b, champsim::data::bits{LOG2_BLOCK_SIZE}>{}, st_full_addr}; // 0xffff'ffc0
 *    address_slice st_page{static_extent<64_b, champsim::data::bits{LOG2_PAGE_SIZE}>{}, st_full_addr}; // 0xffff'f000
 *
 *    address_slice dyn_full_addr{dynamic_extent{64_b,0_b},0xffff'ffff};
 *    address_slice dyn_block{dynamic_extent{64_b, champsim::data::bits{LOG2_BLOCK_SIZE}}, dyn_full_addr}; // 0xffff'ffc0
 *    address_slice dyn_page{dynamic_extent{64_b, champsim::data::bits{LOG2_PAGE_SIZE}}, dyn_full_addr}; // 0xffff'f000
 *
 * \endcode
 *
 * For static extents, it can be useful to explicitly specify the template parameter.
 *
 * \code{.cpp}
 *
 *    address_slice<static_extent<64_b,0_b>> st_full_addr{0xffff'ffff};
 *    address_slice<static_extent<champsim::data::bits{LOG2_BLOCK_SIZE}, 0_b>> st_block{st_full_addr}; // 0x3f
 *    address_slice<static_extent<champsim::data::bits{LOG2_PAGE_SIZE}, 0_b>> st_page{st_full_addr}; // 0xfff
 *
 * \endcode
 *
 * The address slices have a constructor that accepts a uint64_t.
 * No bit shifting is performed in these constructors.
 * The argument is assumed to be in the domain of the slice.
 *
 * \code{.cpp}
 *
 *    champsim::block_number block{0xffff}; // 0x003f'ffc0
 *
 * \endcode
 *
 * Relative slicing is possible with member functions:
 *
 * \code{.cpp}
 *
 *    address_slice<static_extent<24_b,12_b>>::slice(static_extent<8_b,4_b>{}) -> address_slice<static_extent<20_b,16_b>>
 *    address_slice<static_extent<24_b,12_b>>::slice<8_b,4_b>() -> address_slice<static_extent<20_b,16_b>>
 *    address_slice<static_extent<24_b,12_b>>::slice_upper<4_b>() -> address_slice<static_extent<24_b,16_b>>
 *    address_slice<static_extent<24_b,12_b>>::slice_lower<8_b>() -> address_slice<static_extent<20_b,12_b>>
 *
 * \endcode
 *
 * The offset between two addresses can be found with
 *
 * \code{.cpp}
 *
 *    auto champsim::offset(champsim::address base, champsim::address other) -> champsim::address::difference_type
 *    auto champsim::uoffset(champsim::address base, champsim::address other) -> std::make_unsigned_t<champsim::address::difference_type>
 *
 * \endcode
 *
 * The function is a template, so any address slice is accepted, but the two arguments must be of the same type.
 * For the first function, the return type is signed and the conversion is safe against overflows, where it will throw an exception.
 * For the second function, the return type is unsigned and the 'other' argument must succeed 'base', or the function will throw an exception.
 *
 * Address slices also support addition and subtraction with signed integers.
 * The arguments to this arithmetic are in the domain of the type.
 *
 * \code{.cpp}
 *
 *    champsim::block_number block{0xffff}; // 0x003f'ffc0
 *    block += 1; // 0x0040'0000
 *
 * \endcode
 *
 * Two or more slices can be spliced together.
 * Later arguments takes priority over the first, and the result type has an extent that is a superset of all slices.
 *
 * \code{.cpp}
 *
 *    champsim::splice(champsim::page_number{0xaaa}, champsim::page_offset{0xbbb}) == champsim::address{0xaaabbb};
 *
 * \endcode
 *
 * Sometimes, it is necessary to directly manipulate the bits of an address in a way that these strong types do not allow.
 * Additionally, sometimes slices must be used as array indices.
 * In those cases, the address_slice<>::to<T>() function performs a checked cast to the type.
 *
 * \code{.cpp}
 *
 *    champsim::address addr{0xffff'ffff};
 *    class Foo {};
 *    std::array<Foo, 256> foos = {};
 *    auto the_foo = foos.at(addr.slice_lower<8_b>().to<std::size_t>());
 *
 * \endcode
 *
 * \tparam EXTENT One of ``champsim::static_extent<>``, ``champsim::dynamic_extent``, or one of the page- or block-sized extents.
 */
template <typename EXTENT>
class address_slice
{
public:
  /**
   * The extent passed as a template parameter.
   */
  using extent_type = EXTENT;

  /**
   * The underlying representation of the address.
   */
  using underlying_type = uint64_t;

  /**
   * The type of an offset between two addresses.
   */
  using difference_type = std::make_signed_t<underlying_type>;

private:
  using self_type = address_slice<extent_type>;
  constexpr static bool is_static = detail::extent_is_static<extent_type>;

  extent_type extent;

  underlying_type value{};

  template <typename OTHER_EXT>
  static extent_type maybe_dynamic(OTHER_EXT other) noexcept
  {
    if constexpr (std::is_constructible_v<extent_type>) {
      (void)other;
      return extent_type{};
    } else {
      return extent_type{other.upper, other.lower};
    }
  }

  template <typename>
  friend class address_slice;
  friend struct detail::splice_fold_wrapper<extent_type>;

  template <typename E>
  friend constexpr auto offset(address_slice<E> base, address_slice<E> other) -> typename address_slice<E>::difference_type;

  template <typename E>
  friend constexpr auto uoffset(address_slice<E> base, address_slice<E> other) -> std::make_unsigned_t<typename address_slice<E>::difference_type>;

public:
  /**
   * Default-initialize the slice. This constructor is invalid for dynamically-sized extents.
   */
  constexpr address_slice() noexcept(is_static) : address_slice(extent_type{}, underlying_type{}) {}

  /**
   * Initialize the slice with the given raw value. This constructor is invalid for dynamically-sized extents.
   */
  constexpr explicit address_slice(underlying_type val) noexcept(is_static) : address_slice(extent_type{}, val) {}

  /**
   * Initialize the slice with the given slice, shifting and masking if necessary.
   * If this extent is dynamic, it will have the same bounds as the given slice.
   * If the conversion is a widening, the widened bits will be 0.
   */
  template <typename OTHER_EXT>
  constexpr explicit address_slice(const address_slice<OTHER_EXT>& val) noexcept(is_static) : address_slice(maybe_dynamic(val.extent), val)
  {
  }

  /**
   * Initialize the slice with the given slice, shifting and masking if necessary.
   * The extent type can be deduced from the first argument.
   * If the conversion is a widening, the widened bits will be 0.
   */
  template <typename OTHER_EXT>
  constexpr address_slice(extent_type ext, const address_slice<OTHER_EXT>& val) noexcept(is_static)
      : extent(ext), value(((val.value << to_underlying(val.lower_extent())) & bitmask(ext.upper, ext.lower)) >> to_underlying(ext.lower))
  {
    if constexpr (!is_static) {
      if (ext.upper > bits) {
        throw std::invalid_argument{"Upper bound is not representable in the underlying type"};
      }
      if (ext.lower > bits) {
        throw std::invalid_argument{"Lower bound is not representable in the underlying type"};
      }
    }
  }

  /**
   * Initialize the slice with the given value.
   * The extent type can be deduced from the first argument.
   * If the conversion is a widening, the widened bits will be 0.
   */
  constexpr address_slice(extent_type ext, underlying_type val) noexcept(is_static) : extent(ext), value(val & bitmask(data::bits{size(ext)}))
  {
    if constexpr (!is_static) {
      if (ext.upper > bits) {
        throw std::invalid_argument{"Upper bound is not representable in the underlying type"};
      }
      if (ext.lower > bits) {
        throw std::invalid_argument{"Lower bound is not representable in the underlying type"};
      }
    }
  }

  /**
   * The maximum width of any address slice. This is not the width of this slice, which can be found by `slice.upper_extent() - slice.lower_extent()`.
   */
  constexpr static champsim::data::bits bits{std::numeric_limits<underlying_type>::digits};
  static_assert(!is_static || (bounded_upper_v<bits, extent_type> && bounded_lower_v<bits, extent_type>));

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
   * \throws std::invalid_argument If the extents do not match
   */
  [[nodiscard]] constexpr bool operator==(self_type other) const noexcept(is_static)
  {
    if constexpr (!is_static) {
      if (upper_extent() != other.upper_extent()) {
        throw std::invalid_argument{"Upper bounds do not match"};
      }
      if (lower_extent() != other.lower_extent()) {
        throw std::invalid_argument{"Lower bounds do not match"};
      }
    }
    return value == other.value;
  }

  /**
   * Compare with another address slice for ordering.
   *
   * \throws std::invalid_argument If the extents do not match
   */
  [[nodiscard]] constexpr bool operator<(self_type other) const noexcept(is_static)
  {
    if constexpr (!is_static) {
      if (upper_extent() != other.upper_extent()) {
        throw std::invalid_argument{"Upper bounds do not match"};
      }
      if (lower_extent() != other.lower_extent()) {
        throw std::invalid_argument{"Lower bounds do not match"};
      }
    }
    return value < other.value;
  }

  /**
   * Compare with another address slice for inequality.
   *
   * \throws std::invalid_argument If the extents do not match
   */
  [[nodiscard]] constexpr bool operator!=(self_type other) const noexcept(is_static) { return !(*this == other); }

  /**
   * Compare with another address slice for ordering.
   *
   * \throws std::invalid_argument If the extents do not match
   */
  [[nodiscard]] constexpr bool operator<=(self_type other) const noexcept(is_static) { return *this < other || *this == other; }

  /**
   * Compare with another address slice for ordering.
   *
   * \throws std::invalid_argument If the extents do not match
   */
  [[nodiscard]] constexpr bool operator>(self_type other) const noexcept(is_static) { return !(value <= other.value); }

  /**
   * Compare with another address slice for ordering.
   *
   * \throws std::invalid_argument If the extents do not match
   */
  [[nodiscard]] constexpr bool operator>=(self_type other) const noexcept(is_static) { return *this > other || *this == other; }

  /**
   * Increment the slice in place by the given amount.
   * The delta is interpreted in the domain of the slice. That is, the delta need not be scaled by ``1 << slice.lower_extent()``.
   */
  constexpr self_type& operator+=(difference_type delta)
  {
    value += static_cast<underlying_type>(delta);
    value &= bitmask(data::bits{size(extent)});
    return *this;
  }

  /**
   * Increment the slice in place by the given amount.
   * The delta is interpreted in the domain of the slice. That is, the delta need not be scaled by ``1 << slice.lower_extent()``.
   */
  constexpr self_type& operator+=(champsim::data::bytes delta) { return operator+=(delta.count()); }

  /**
   * Increment the slice by the given amount.
   * The delta is interpreted in the domain of the slice. That is, the delta need not be scaled by ``1 << slice.lower_extent()``.
   */
  [[nodiscard]] constexpr self_type operator+(difference_type delta) const
  {
    self_type retval = *this;
    retval += delta;
    return retval;
  }

  /**
   * Increment the slice by the given amount.
   * The delta is interpreted in the domain of the slice. That is, the delta need not be scaled by ``1 << slice.lower_extent()``.
   */
  [[nodiscard]] constexpr self_type operator+(champsim::data::bytes delta) const { return operator+(delta.count()); }

  /**
   * Decrement the slice in place by the given amount.
   * The delta is interpreted in the domain of the slice. That is, the delta need not be scaled by ``1 << slice.lower_extent()``.
   */
  constexpr self_type& operator-=(difference_type delta) { return operator+=(-delta); }

  /**
   * Decrement the slice in place by the given amount.
   * The delta is interpreted in the domain of the slice. That is, the delta need not be scaled by ``1 << slice.lower_extent()``.
   */
  constexpr self_type& operator-=(champsim::data::bytes delta) { return operator-=(delta.count()); }

  /**
   * Decrement the slice by the given amount.
   * The delta is interpreted in the domain of the slice. That is, the delta need not be scaled by ``1 << slice.lower_extent()``.
   */
  [[nodiscard]] constexpr self_type operator-(difference_type delta) const { return operator+(-delta); }

  /**
   * Decrement the slice by the given amount.
   * The delta is interpreted in the domain of the slice. That is, the delta need not be scaled by ``1 << slice.lower_extent()``.
   */
  [[nodiscard]] constexpr self_type operator-(champsim::data::bytes delta) const { return operator-(delta.count()); }

  /**
   * Increment the slice by one.
   */
  constexpr self_type& operator++() { return operator+=(1); }

  /**
   * Increment the slice by one.
   */
  constexpr self_type operator++(int)
  {
    self_type retval = *this;
    operator++();
    return retval;
  }

  /**
   * Decrement the slice by one.
   */
  constexpr self_type& operator--() { return operator-=(1); }

  /**
   * Decrement the slice by one.
   */
  constexpr self_type operator--(int)
  {
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
  [[nodiscard]] auto slice(SUB_EXTENT subextent) const noexcept(is_static&& detail::extent_is_static<SUB_EXTENT>)
  {
    auto new_ext = relative_extent(extent, subextent);
    return address_slice<decltype(new_ext)>{new_ext, value >> to_underlying(subextent.lower)};
  }

  /**
   * Perform a slice on this address. The given extent should be relative to this slice's extent.
   * This is a synonym for ``slice(static_extent<new_upper, new_lower>{})``.
   */
  template <champsim::data::bits new_upper, champsim::data::bits new_lower>
  [[nodiscard]] auto slice() const noexcept
  {
    return slice(static_extent<new_upper, new_lower>{});
  }

  /**
   * Slice the upper bits, ending with the given bit relative to the lower extent of this.
   * If this slice is statically-sized, the result will be statically-sized.
   */
  template <champsim::data::bits new_lower>
  [[nodiscard]] auto slice_upper() const noexcept
  {
    return slice<bits, new_lower>();
  }

  /**
   * Slice the lower bits, ending with the given bit relative to the lower extent of this.
   * If this slice is statically-sized, the result will be statically-sized.
   */
  template <champsim::data::bits new_upper>
  [[nodiscard]] auto slice_lower() const noexcept
  {
    using namespace champsim::data::data_literals;
    return slice<new_upper, 0_b>();
  }

  /**
   * Slice the upper bits, ending with the given bit relative to the lower extent of this.
   * The result of this will always be runtime-sized.
   */
  [[nodiscard]] auto slice_upper(champsim::data::bits new_lower) const { return slice(dynamic_extent{bits, new_lower}); }

  /**
   * Slice the lower bits, ending with the given bit relative to the lower extent of this.
   * The result of this will always be runtime-sized.
   */
  [[nodiscard]] auto slice_lower(champsim::data::bits new_upper) const { return slice(dynamic_extent{new_upper, champsim::data::bits{}}); }

  /**
   * Split the slice into an upper and lower slice.
   */
  template <champsim::data::bits split_loc>
  [[nodiscard]] auto split() const
  {
    return std::pair{slice_upper<split_loc>(), slice_lower<split_loc>()};
  }

  /**
   * Split the slice into an upper and lower slice.
   */
  [[nodiscard]] auto split(champsim::data::bits split_loc) const { return std::pair{slice_upper(split_loc), slice_lower(split_loc)}; }

  /**
   * Get the upper portion of the extent.
   */
  [[nodiscard]] constexpr auto upper_extent() const noexcept
  {
    if constexpr (is_static) {
      return extent_type::upper;
    } else {
      return extent.upper;
    }
  }

  /**
   * Get the lower portion of the extent.
   */
  [[nodiscard]] constexpr auto lower_extent() const noexcept
  {
    if constexpr (is_static) {
      return extent_type::lower;
    } else {
      return extent.lower;
    }
  }
};

template <typename Extent>
constexpr auto offset(address_slice<Extent> base, address_slice<Extent> other) -> typename address_slice<Extent>::difference_type
{
  using difference_type = typename address_slice<Extent>::difference_type;

  const bool result_sign = (base <= other);
  auto abs_diff = result_sign ? uoffset(base, other) : uoffset(other, base);
  if (abs_diff > std::numeric_limits<difference_type>::max()) {
    throw std::overflow_error{
        fmt::format("The offset between {} and {} cannot be represented in the difference type. Consider using champsim::uoffset() instead.", base, other)};
  }
  return result_sign ? static_cast<difference_type>(abs_diff) : -static_cast<difference_type>(abs_diff);
}

template <typename Extent>
constexpr auto uoffset(address_slice<Extent> base, address_slice<Extent> other) -> std::make_unsigned_t<typename address_slice<Extent>::difference_type>
{
  if (base > other) {
    throw std::overflow_error{fmt::format("The offset between {} and {} cannot be represented in the difference type.", base, other)};
  }

  using difference_type = std::make_unsigned_t<typename address_slice<Extent>::difference_type>;
  return difference_type{other.value - base.value};
}

namespace detail
{
template <typename Extent>
struct splice_fold_wrapper {
  using extent_type = Extent;
  using value_type = typename address_slice<extent_type>::underlying_type;

  extent_type extent;
  value_type underlying;

  explicit splice_fold_wrapper(address_slice<extent_type> addr) noexcept : splice_fold_wrapper(addr.extent, addr.value) {}
  splice_fold_wrapper(extent_type ext, value_type und) noexcept : extent(ext), underlying(und) {}

  template <typename OtherExtent>
  auto operator+(splice_fold_wrapper<OtherExtent> other) const
  {
    auto return_extent = extent_union(this->extent, other.extent);
    auto lhs_shifted = champsim::translate(underlying, extent, return_extent);
    auto rhs_shifted = champsim::translate(other.underlying, other.extent, return_extent);
    auto window = other.extent >> return_extent.lower;
    return splice_fold_wrapper<decltype(return_extent)>{return_extent, splice_bits(lhs_shifted, rhs_shifted, window.upper, window.lower)};
  }

  auto address() const noexcept { return address_slice{extent, underlying}; }
};
} // namespace detail

template <typename... Extents>
constexpr auto splice(address_slice<Extents>... slices)
{
  return (detail::splice_fold_wrapper<Extents>{slices} + ...).address();
}

template <typename Ostr, typename extent_type>
Ostr& operator<<(Ostr& stream, const champsim::address_slice<extent_type>& addr)
{
  auto addr_flags = std::ios_base::hex | std::ios_base::showbase | std::ios_base::internal;
  auto addr_mask = std::ios_base::basefield | std::ios_base::showbase | std::ios_base::adjustfield;

  auto oldflags = stream.setf(addr_flags, addr_mask);
  auto oldfill = stream.fill('0');

  stream << addr.template to<typename champsim::address_slice<extent_type>::underlying_type>();

  stream.setf(oldflags, addr_mask);
  stream.fill(oldfill);

  return stream;
}
} // namespace champsim

template <typename Extent>
struct fmt::formatter<champsim::address_slice<Extent>> {
  using addr_type = champsim::address_slice<Extent>;
  std::size_t len = std::numeric_limits<std::size_t>::max();

  constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator
  {
    auto [ret_ptr, ec] = std::from_chars(ctx.begin(), ctx.end(), len);
    // Check if reached the end of the range:
    if (ec == std::errc::result_out_of_range || (ret_ptr != ctx.end() && *ret_ptr != '}'))
      throw fmt::format_error("invalid format");
    return ret_ptr;
  }

  auto format(const addr_type& addr, fmt::format_context& ctx) const -> fmt::format_context::iterator
  {
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
