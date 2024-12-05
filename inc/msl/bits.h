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

#ifndef MSL_BITS_H
#define MSL_BITS_H

#include <cstdint>
#include <limits>

#include "util/to_underlying.h"
#include "util/units.h"

namespace champsim::msl
{
/**
 * Compute the base-2 logarithm of an integer.
 */
template <typename T>
constexpr auto lg2(T n)
{
  std::make_unsigned_t<T> result{};
  while (n >>= 1) {
    ++result;
  }
  return result;
}

/**
 * Compute the smallest power of 2 not less than the given integer.
 */
template <typename T>
constexpr T next_pow2(T n)
{
  n--;
  for (unsigned i = 0; i < lg2(std::numeric_limits<T>::digits); ++i) {
    n |= n >> (1u << i);
  }
  n++;
  return n;
}

/**
 * A backport of ``std::has_single_bit()``.
 */
template <typename T>
constexpr bool is_power_of_2(T n)
{
  return (n == T{1} << lg2(n));
}

/**
 * Compute an integer power.
 * This function may overflow very easily. Use only for small bases or very small exponents.
 */
constexpr long long ipow(long long base, unsigned exp)
{
  long long result = 1;
  for (;;) {
    if (exp & 1)
      result *= base;
    exp >>= 1;
    if (!exp)
      break;
    base *= base;
  }

  return result;
}

/**
 * Produce a mask that can be used in a bitwise-and operation to select particular bits of a number.
 *
 * \code{.cpp}
 *     0xffff & bitmask(8_b,4_b) == 0xf0;
 * \endcode
 *
 * If ``end`` is not specified, it is defaulted to 0, that is, the least-significant bit.
 *
 * \param begin The most-significant bit of the mask, not inclusive.
 * \param end The least-significant bit of the mask, inclusive.
 *
 * \note This should not be used as a mask generator for address types. Use champsim::address_slice instead.
 */
constexpr uint64_t bitmask(champsim::data::bits begin, champsim::data::bits end)
{
  using underlying_type = std::underlying_type_t<champsim::data::bits>;
  auto begin_val = champsim::to_underlying(begin);
  auto end_val = champsim::to_underlying(end);

  if (begin_val - end_val >= std::numeric_limits<underlying_type>::digits) {
    return std::numeric_limits<underlying_type>::max();
  }
  return ((underlying_type{1} << (begin_val - end_val)) - 1) << end_val;
}

/**
 * \overload
 */
constexpr uint64_t bitmask(champsim::data::bits begin) { return bitmask(begin, champsim::data::bits{}); }

/**
 * \overload
 * \deprecated Users should prefer the overload with ``champsim::data::bits`` parameters.
 */
[[deprecated]] constexpr uint64_t bitmask(std::size_t begin, std::size_t end = 0) { return bitmask(champsim::data::bits{begin}, champsim::data::bits{end}); }

/**
 * Join two values, selecting some bits from one value and some bits from another.
 *
 * \code{.cpp}
 *     splice_bits(0xaaaa, 0xbbbb, 8_b, 4_b) == 0xaaba;
 * \endcode
 *
 * If ``end`` is not specified, it is defaulted to 0, that is, the least-significant bit.
 *
 * \param upper The operand from which to take the not-selected bits.
 * \param lower The operand from which to take the selected bits.
 * \param bits_upper The most-significant bit of the mask, not inclusive.
 * \param bits_lower The least-significant bit of the mask, inclusive.
 *
 * \note This is an implementation detail of champsim::splice(), which should be preferred for address types. This function is provided for general use.
 */
template <typename T>
constexpr auto splice_bits(T upper, T lower, champsim::data::bits bits_upper, champsim::data::bits bits_lower)
{
  return (upper & ~bitmask(bits_upper, bits_lower)) | (lower & bitmask(bits_upper, bits_lower));
}

/**
 * \overload
 */
template <typename T>
constexpr auto splice_bits(T upper, T lower, champsim::data::bits bits)
{
  return splice_bits(upper, lower, bits, champsim::data::bits{});
}

/**
 * \overload
 * \deprecated Users should prefer the overload with ``champsim::data::bits`` parameters.
 */
template <typename T>
[[deprecated]] constexpr auto splice_bits(T upper, T lower, std::size_t bits_upper, std::size_t bits_lower)
{
  return splice_bits(upper, lower, champsim::data::bits{bits_upper}, champsim::data::bits{bits_lower});
}

/**
 * \overload
 * \deprecated Users should prefer the overload with ``champsim::data::bits`` parameters.
 */
template <typename T>
[[deprecated]] constexpr auto splice_bits(T upper, T lower, std::size_t bits)
{
  return splice_bits(upper, lower, champsim::data::bits{bits});
}
} // namespace champsim::msl

#endif
