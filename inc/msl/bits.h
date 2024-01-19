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

namespace champsim::msl
{
template <typename T>
constexpr auto lg2(T n)
{
  std::make_unsigned_t<T> result{};
  while (n >>= 1) {
    ++result;
  }
  return result;
}

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

template <typename T>
constexpr bool is_power_of_2(T n)
{
  return (n == T{1} << lg2(n));
}

/**
 * Compute an integer power
 * This function may overflow very easily. Use only for small bases or very small exponents.
 */
constexpr long long static ipow(long long base, unsigned exp)
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

constexpr uint64_t bitmask(std::size_t begin, std::size_t end = 0)
{
  if (begin - end >= std::numeric_limits<uint64_t>::digits) {
    return std::numeric_limits<uint64_t>::max();
  }
  return ((1ULL << (begin - end)) - 1) << end;
}

template <typename T>
constexpr auto splice_bits(T upper, T lower, std::size_t bits_upper, std::size_t bits_lower)
{
  return (upper & ~bitmask(bits_upper, bits_lower)) | (lower & bitmask(bits_upper, bits_lower));
}

template <typename T>
constexpr auto splice_bits(T upper, T lower, std::size_t bits)
{
  return splice_bits(upper, lower, bits, 0);
}
} // namespace champsim::msl

#endif
