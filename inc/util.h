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

#ifndef UTIL_H
#define UTIL_H

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <vector>

#include "msl/bits.h"
#include "msl/lru_table.h"

template <typename T>
struct is_valid {
  using argument_type = T;
  is_valid() {}
  bool operator()(const argument_type& test) { return test.valid; }
};

template <typename T>
struct is_valid<std::optional<T>> {
  bool operator()(const std::optional<T>& test) { return test.has_value(); }
};

template <typename T, typename = void>
struct eq_addr
{
    using argument_type = T;
    using addr_type = decltype(T::address);
    const addr_type match_addr;
    const std::size_t shamt;

    explicit eq_addr(addr_type addr, std::size_t shift_bits = 0) : match_addr(addr), shamt(shift_bits) {}
    explicit eq_addr(const argument_type &elem, std::size_t shift_bits = 0) : eq_addr(elem.address, shift_bits) {}

    bool operator()(const argument_type &test)
    {
        is_valid<argument_type> validtest;
        return validtest(test) && (test.address >> shamt) == (match_addr >> shamt);
    }
};

// Specialization for types that include a member T::asid
template <typename T>
struct eq_addr<T, std::void_t<decltype(T::asid)>>
{
    using argument_type = T;
    using addr_type = decltype(T::address);
    using asid_type = decltype(T::asid);

    const asid_type match_asid;
    const addr_type match_addr;
    const std::size_t shamt;

    eq_addr(asid_type asid, addr_type addr, std::size_t shift_bits = 0) : match_asid(asid), match_addr(addr), shamt(shift_bits) {}
    explicit eq_addr(const argument_type &elem, std::size_t shift_bits = 0) : eq_addr(elem.asid, elem.address, shift_bits) {}

    bool operator()(const argument_type &test)
    {
        is_valid<argument_type> validtest;
        return validtest(test) && test.asid == match_asid && (test.address >> shamt) == (match_addr >> shamt);
    }
};

namespace champsim
{
using msl::bitmask;
using msl::lg2;
using msl::lru_table;
using msl::splice_bits;

template <typename It>
std::pair<It, It> get_span(It begin, It end, typename std::iterator_traits<It>::difference_type sz)
{
  assert(std::distance(begin, end) >= 0);
  assert(sz >= 0);
  auto distance = std::min(std::distance(begin, end), sz);
  return {begin, std::next(begin, distance)};
}

template <typename It, typename F>
std::pair<It, It> get_span_p(It begin, It end, typename std::iterator_traits<It>::difference_type sz, F&& func)
{
  auto [span_begin, span_end] = get_span(begin, end, sz);
  return {span_begin, std::find_if_not(span_begin, span_end, std::forward<F>(func))};
}

} // namespace champsim

#endif
