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

template <std::size_t, std::size_t>
class address_slice;

// Convenience definitions
using address = address_slice<std::numeric_limits<uint64_t>::digits, 0>;
using block_number = address_slice<std::numeric_limits<uint64_t>::digits, LOG2_BLOCK_SIZE>;
using block_offset = address_slice<LOG2_BLOCK_SIZE, 0>;
using page_number = address_slice<std::numeric_limits<uint64_t>::digits, LOG2_PAGE_SIZE>;
using page_offset = address_slice<LOG2_PAGE_SIZE, 0>;
} // namespace champsim

#endif
