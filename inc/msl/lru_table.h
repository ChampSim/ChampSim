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

#ifndef MSL_LRU_TABLE_H
#define MSL_LRU_TABLE_H

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "msl/bits.h"

namespace champsim::msl
{
namespace detail
{
template <typename T>
struct table_indexer {
  auto operator()(const T& t) const { return t.index(); }
};

template <typename T>
struct table_tagger {
  auto operator()(const T& t) const { return t.tag(); }
};
} // namespace detail

template <typename T, typename SetProj = detail::table_indexer<T>, typename TagProj = detail::table_tagger<T>>
class lru_table
{
public:
  using value_type = T;

private:
  struct block_t {
    uint64_t last_used = 0;
    value_type data;
  };
  using block_vec_type = std::vector<block_t>;

  SetProj set_projection;
  TagProj tag_projection;

  std::size_t NUM_SET, NUM_WAY;
  uint64_t access_count = 0;
  block_vec_type block{NUM_SET * NUM_WAY};

  auto get_set_span(const value_type& elem)
  {
    using diff_type = typename block_vec_type::difference_type;
    auto set_idx = static_cast<diff_type>(set_projection(elem) & bitmask(lg2(NUM_SET)));
    auto set_begin = std::next(std::begin(block), set_idx * static_cast<diff_type>(NUM_WAY));
    auto set_end = std::next(set_begin, static_cast<diff_type>(NUM_WAY));
    return std::pair{set_begin, set_end};
  }

  auto match_func(const value_type& elem)
  {
    return [tag = tag_projection(elem), proj = this->tag_projection](const block_t& x) {
      return x.last_used > 0 && proj(x.data) == tag;
    };
  }

  template <typename U>
  auto match_and_check(U tag)
  {
    return [tag, proj = this->tag_projection](const auto& x, const auto& y) {
      auto x_valid = x.last_used > 0;
      auto y_valid = y.last_used > 0;
      auto x_match = proj(x.data) == tag;
      auto y_match = proj(y.data) == tag;
      auto cmp_lru = x.last_used < y.last_used;
      return !x_valid || (y_valid && ((!x_match && y_match) || ((x_match == y_match) && cmp_lru)));
    };
  }

public:
  std::optional<value_type> check_hit(const value_type& elem)
  {
    auto [set_begin, set_end] = get_set_span(elem);
    auto hit = std::find_if(set_begin, set_end, match_func(elem));

    if (hit == set_end)
      return std::nullopt;

    hit->last_used = ++access_count;
    return hit->data;
  }

  void fill(const value_type& elem)
  {
    auto tag = tag_projection(elem);
    auto [set_begin, set_end] = get_set_span(elem);
    auto [miss, hit] = std::minmax_element(set_begin, set_end, match_and_check(tag));

    if (tag_projection(hit->data) == tag)
      *hit = {++access_count, elem};
    else
      *miss = {++access_count, elem};
  }

  std::optional<value_type> invalidate(const value_type& elem)
  {
    auto [set_begin, set_end] = get_set_span(elem);
    auto hit = std::find_if(set_begin, set_end, match_func(elem));

    if (hit == set_end)
      return std::nullopt;

    return std::exchange(*hit, {}).data;
  }

  lru_table(std::size_t sets, std::size_t ways, SetProj set_proj, TagProj tag_proj)
      : set_projection(set_proj), tag_projection(tag_proj), NUM_SET(sets), NUM_WAY(ways)
  {
    assert(sets > 0);
    assert(ways > 0);
    assert(sets == (1ull << lg2(sets)));
  }

  lru_table(std::size_t sets, std::size_t ways, SetProj set_proj) : lru_table(sets, ways, set_proj, {}) {}
  lru_table(std::size_t sets, std::size_t ways) : lru_table(sets, ways, {}, {}) {}
};
} // namespace champsim::msl

#endif
