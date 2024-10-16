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

#ifndef UTIL_SPAN_H
#define UTIL_SPAN_H

#include <algorithm>
#include <cassert>
#include <iterator>
#include <limits>

#include "bandwidth.h"

namespace champsim
{
template <typename It>
std::pair<It, It> get_span(It begin, It end, bandwidth sz)
{
  assert(std::distance(begin, end) >= 0);
  assert(sz.amount_remaining() >= 0);
  auto distance = std::min(std::distance(begin, end), sz.amount_remaining());
  return {begin, std::next(begin, distance)};
}

template <typename It, typename F>
std::pair<It, It> get_span_p(It begin, It end, bandwidth sz, F&& func)
{
  auto [span_begin, span_end] = get_span(begin, end, sz);
  return {span_begin, std::find_if_not(span_begin, span_end, std::forward<F>(func))};
}

template <typename It, typename F>
std::pair<It, It> get_span_p(It begin, It end, F&& func)
{
  return {begin, std::find_if_not(begin, end, std::forward<F>(func))};
}
} // namespace champsim

#endif
