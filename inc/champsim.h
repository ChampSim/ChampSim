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

#include <cstdlib>
#include <cstdint>
#include <exception>
#include "util/bits.h"

inline constexpr unsigned BLOCK_SIZE = 64;
inline constexpr unsigned PAGE_SIZE = 4096;
inline constexpr uint64_t STAT_PRINTING_PERIOD = 10000000;
inline constexpr std::size_t NUM_CPUS = 1;
inline constexpr auto LOG2_BLOCK_SIZE = champsim::lg2(BLOCK_SIZE);
inline constexpr auto LOG2_PAGE_SIZE = champsim::lg2(PAGE_SIZE);
inline constexpr uint64_t DRAM_IO_FREQ = 3200;
inline constexpr std::size_t DRAM_CHANNELS = 1;
inline constexpr std::size_t DRAM_RANKS = 1;
inline constexpr std::size_t DRAM_BANKS = 8;
inline constexpr std::size_t DRAM_ROWS = 65536;
inline constexpr std::size_t DRAM_COLUMNS = 128;
inline constexpr std::size_t DRAM_CHANNEL_WIDTH = 8;
inline constexpr std::size_t DRAM_WQ_SIZE = 64;
inline constexpr std::size_t DRAM_RQ_SIZE = 64;

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
} // namespace champsim

#endif
