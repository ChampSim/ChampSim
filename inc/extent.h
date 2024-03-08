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

#ifndef EXTENT_H
#define EXTENT_H

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <limits>
#include <type_traits>

#include "util/to_underlying.h"
#include "util/units.h"

namespace champsim
{
/**
 * An extent with runtime size
 */
struct dynamic_extent {
  /**
   * The upper limit of the extent. Generally, this is not inclusive.
   */
  champsim::data::bits upper;

  /**
   * The lower limit of the extent. This is generally inclusive.
   */
  champsim::data::bits lower;

  /**
   * Initialize the extent with the given upper and lower extents. The upper must be greater than or equal to the lower.
   */
  constexpr dynamic_extent(champsim::data::bits up, champsim::data::bits low) : upper(up), lower(low) { assert(upper >= lower); }

  /**
   * Initialize the extent with a lower extent and size.
   */
  constexpr dynamic_extent(champsim::data::bits low, std::size_t size) : dynamic_extent(low + champsim::data::bits{size}, low) {}
};

/**
 * An extent that is always the size of a page number
 */
struct page_number_extent : dynamic_extent {
  /**
   * This extent can only be default-initialized.
   * It will have ``upper == champsim::address::bits`` and ``lower == LOG2_PAGE_SIZE``.
   */
  page_number_extent();
};

/**
 * An extent that is always the size of a page offset
 */
struct page_offset_extent : dynamic_extent {
  /**
   * This extent can only be default-initialized.
   * It will have ``upper == LOG2_PAGE_SIZE`` and ``lower == 0``.
   */
  page_offset_extent();
};

/**
 * An extent that is always the size of a block number
 */
struct block_number_extent : dynamic_extent {
  /**
   * This extent can only be default-initialized.
   * It will have ``upper == champsim::address::bits`` and ``lower == LOG2_BLOCK_SIZE``.
   */
  block_number_extent();
};

/**
 * An extent that is always the size of a block offset
 */
struct block_offset_extent : dynamic_extent {
  /**
   * This extent can only be default-initialized.
   * It will have ``upper == LOG2_BLOCK_SIZE`` and ``lower == 0``.
   */
  block_offset_extent();
};

/**
 * An extent with compile-time size
 */
template <champsim::data::bits UP, champsim::data::bits LOW>
struct static_extent {
  /**
   * The upper limit of the extent. Generally, this is not inclusive.
   */
  constexpr static champsim::data::bits upper{UP};

  /**
   * The lower limit of the extent. This is generally inclusive.
   */
  constexpr static champsim::data::bits lower{LOW};
};

/**
 * Give the width of the extent. For static_extent, this function can be constexpr.
 */
std::size_t size(dynamic_extent ext);
std::size_t size(page_number_extent ext);
std::size_t size(page_offset_extent ext);
std::size_t size(block_number_extent ext);
std::size_t size(block_offset_extent ext);

template <champsim::data::bits UP, champsim::data::bits LOW>
constexpr std::size_t size(static_extent<UP, LOW> ext)
{
  return to_underlying(ext.upper) - to_underlying(ext.lower);
}

namespace detail
{
template <typename E>
constexpr bool extent_is_static = false;

template <champsim::data::bits UP, champsim::data::bits LOW>
constexpr bool extent_is_static<static_extent<UP, LOW>> = true;
} // namespace detail

/**
 * Find the smallest extent that contains both given extents
 */
template <typename LHS_EXTENT, typename RHS_EXTENT>
auto extent_union(LHS_EXTENT lhs, RHS_EXTENT rhs)
{
  if constexpr (detail::extent_is_static<std::decay_t<LHS_EXTENT>> && detail::extent_is_static<std::decay_t<RHS_EXTENT>>) {
    return static_extent<std::max(lhs.upper, rhs.upper), std::min(lhs.lower, rhs.lower)>{};
  } else {
    return dynamic_extent{std::max(lhs.upper, rhs.upper), std::min(lhs.lower, rhs.lower)};
  }
}

/**
 * Select a portion of the superextent
 */
template <typename LHS_EXTENT, typename RHS_EXTENT>
auto relative_extent(LHS_EXTENT superextent, RHS_EXTENT subextent)
{
  if constexpr (detail::extent_is_static<std::decay_t<LHS_EXTENT>> && detail::extent_is_static<std::decay_t<RHS_EXTENT>>) {
    constexpr data::bits superextent_size{size(superextent)};
    constexpr data::bits superextent_upper{superextent.lower + std::min(subextent.upper, superextent_size)};
    constexpr data::bits superextent_lower{superextent.lower + std::min(subextent.lower, superextent_size)};
    return static_extent<superextent_upper, superextent_lower>{};
  } else {
    const data::bits superextent_size{size(superextent)};
    const data::bits superextent_upper{superextent.lower + std::min(subextent.upper, superextent_size)};
    const data::bits superextent_lower{superextent.lower + std::min(subextent.lower, superextent_size)};
    return dynamic_extent{superextent_upper, superextent_lower};
  }
}

/**
 * True if the extent is statically known to be bounded above by the given value
 */
template <auto UP, typename EXTENT>
constexpr bool bounded_upper_v = false;

template <auto UP, champsim::data::bits SUB_UP, champsim::data::bits SUB_LOW>
constexpr bool bounded_upper_v<UP, static_extent<SUB_UP, SUB_LOW>> = (SUB_UP <= champsim::data::bits{UP});

template <auto UP, typename EXTENT>
struct bounded_upper : std::bool_constant<bounded_upper_v<UP, EXTENT>> {
};

/**
 * True if the extent is statically known to be bounded below by the given value
 */
template <auto LOW, typename EXTENT>
constexpr bool bounded_lower_v = false;

template <auto LOW, champsim::data::bits SUB_UP, champsim::data::bits SUB_LOW>
constexpr bool bounded_lower_v<LOW, static_extent<SUB_UP, SUB_LOW>> = (SUB_LOW <= champsim::data::bits{LOW});

template <auto LOW, typename EXTENT>
struct bounded_lower : std::bool_constant<bounded_lower_v<LOW, EXTENT>> {
};

template <typename T, typename FROM, typename TO>
T translate(T value, FROM from, TO to)
{
  if (from.lower <= to.lower) {
    return value >> (to_underlying(to.lower) - to_underlying(from.lower));
  } else {
    return value << (to_underlying(from.lower) - to_underlying(to.lower));
  }
}
} // namespace champsim

template <typename EXT>
constexpr auto operator>>(EXT ext, champsim::data::bits shamt)
{
  champsim::data::bits new_up{champsim::to_underlying(ext.upper) - champsim::to_underlying(shamt)};
  champsim::data::bits new_low{champsim::to_underlying(ext.lower) - champsim::to_underlying(shamt)};
  return champsim::dynamic_extent{new_up, new_low};
}

template <typename EXT>
constexpr auto operator<<(EXT ext, champsim::data::bits shamt)
{
  champsim::data::bits new_up{champsim::to_underlying(ext.upper) + champsim::to_underlying(shamt)};
  champsim::data::bits new_low{champsim::to_underlying(ext.lower) + champsim::to_underlying(shamt)};
  return champsim::dynamic_extent{new_up, new_low};
}

#endif
