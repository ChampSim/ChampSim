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

extern std::size_t NUM_CPUS;
extern unsigned BLOCK_SIZE;
extern unsigned PAGE_SIZE;
extern unsigned LOG2_BLOCK_SIZE;
extern unsigned LOG2_PAGE_SIZE;
extern uint64_t DRAM_IO_FREQ;
extern std::size_t DRAM_CHANNELS;
extern std::size_t DRAM_RANKS;
extern std::size_t DRAM_BANKS;
extern std::size_t DRAM_ROWS;
extern std::size_t DRAM_COLUMNS;
extern std::size_t DRAM_CHANNEL_WIDTH;
extern std::size_t DRAM_WQ_SIZE;
extern std::size_t DRAM_RQ_SIZE;

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
