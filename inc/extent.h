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
#include <cassert>
#include <type_traits>

namespace champsim {
struct dynamic_extent
{
  std::size_t upper;
  std::size_t lower;

  dynamic_extent(std::size_t up, std::size_t low) : upper(up), lower(low)
  {
    assert(up >= low);
  }
};

template <std::size_t UP, std::size_t LOW>
struct static_extent
{
  constexpr static std::size_t upper{UP};
  constexpr static std::size_t lower{LOW};
};

namespace detail
{
  template <typename E>
  constexpr bool extent_is_static = false;

  template <std::size_t UP, std::size_t LOW>
  constexpr bool extent_is_static<static_extent<UP, LOW>> = true;
}

template <typename LHS_EXTENT, typename RHS_EXTENT>
auto extent_union(LHS_EXTENT lhs, RHS_EXTENT rhs)
{
  if constexpr (detail::extent_is_static<std::decay_t<LHS_EXTENT>> && detail::extent_is_static<std::decay_t<RHS_EXTENT>>) {
    return static_extent<std::max(lhs.upper, rhs.upper), std::min(lhs.lower, rhs.lower)>{};
  } else {
    return dynamic_extent{std::max(lhs.upper, rhs.upper), std::min(lhs.lower, rhs.lower)};
  }
}
}

#endif
