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

#ifndef CHAMPSIM_H
#define CHAMPSIM_H

#include <cstdint>
#include <exception>
#include <limits>

#include "champsim_constants.h"
#include "extent.h"
#include "util/ratio.h"

namespace champsim
{
struct deadlock : public std::exception {
  const uint32_t which;
  explicit deadlock(uint32_t cpu) : which(cpu) {}
};

#ifdef DEBUG_PRINT
constexpr bool debug_print = true;
#else
constexpr bool debug_print = false;
#endif

template <typename Extent>
class address_slice;

namespace data
{
template <typename Rep, typename Unit>
class size;

// Convenience definitions
using bytes = size<long long, std::ratio<1>>;
using kibibytes = size<long long, kibi>;
using mebibytes = size<long long, mebi>;
using gibibytes = size<long long, gibi>;
using tebibytes = size<long long, tebi>;
using blocks = size<long long, std::ratio<BLOCK_SIZE>>;
using pages = size<long long, std::ratio<PAGE_SIZE>>;
} // namespace data

// Convenience definitions
using address = address_slice<static_extent<std::numeric_limits<uint64_t>::digits, 0>>;
using block_number = address_slice<static_extent<std::numeric_limits<uint64_t>::digits, LOG2_BLOCK_SIZE>>;
using block_offset = address_slice<static_extent<LOG2_BLOCK_SIZE, 0>>;
using page_number = address_slice<static_extent<std::numeric_limits<uint64_t>::digits, LOG2_PAGE_SIZE>>;
using page_offset = address_slice<static_extent<LOG2_PAGE_SIZE, 0>>;

auto lowest_address_for_size(data::bytes sz) -> address;
} // namespace champsim

#endif
