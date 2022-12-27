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

#ifndef DRAM_H
#define DRAM_H

#include <array>
#include <cmath>
#include <limits>

#include "champsim_constants.h"
#include "memory_class.h"
#include "operable.h"
#include "util.h"

// these values control when to send out a burst of writes
constexpr std::size_t DRAM_WRITE_HIGH_WM = ((DRAM_WQ_SIZE * 7) >> 3);         // 7/8th
constexpr std::size_t DRAM_WRITE_LOW_WM = ((DRAM_WQ_SIZE * 6) >> 3);          // 6/8th
constexpr std::size_t MIN_DRAM_WRITES_PER_SWITCH = ((DRAM_WQ_SIZE * 1) >> 2); // 1/4

namespace detail
{
// https://stackoverflow.com/a/31962570
constexpr int32_t ceil(float num)
{
  return (static_cast<float>(static_cast<int32_t>(num)) == num) ? static_cast<int32_t>(num) : static_cast<int32_t>(num) + ((num > 0) ? 1 : 0);
}
} // namespace detail

struct BANK_REQUEST {
  bool valid = false, row_buffer_hit = false;

  std::size_t open_row = std::numeric_limits<uint32_t>::max();

  uint64_t event_cycle = 0;

  std::vector<PACKET>::iterator pkt;
};

struct DRAM_CHANNEL {
  std::vector<PACKET> WQ{DRAM_WQ_SIZE};
  std::vector<PACKET> RQ{DRAM_RQ_SIZE};

  std::array<BANK_REQUEST, DRAM_RANKS* DRAM_BANKS> bank_request = {};
  std::array<BANK_REQUEST, DRAM_RANKS* DRAM_BANKS>::iterator active_request = std::end(bank_request);

  uint64_t dbus_cycle_available = 0, dbus_cycle_congested = 0, dbus_count_congested = 0;

  bool write_mode = false;

  unsigned WQ_ROW_BUFFER_HIT = 0, WQ_ROW_BUFFER_MISS = 0, RQ_ROW_BUFFER_HIT = 0, RQ_ROW_BUFFER_MISS = 0, WQ_FULL = 0;
};

class MEMORY_CONTROLLER : public champsim::operable, public MemoryRequestConsumer
{
public:
  // DRAM_IO_FREQ defined in champsim_constants.h
  const static uint64_t tRP = detail::ceil(1.0 * tRP_DRAM_NANOSECONDS * DRAM_IO_FREQ / 1000);
  const static uint64_t tRCD = detail::ceil(1.0 * tRCD_DRAM_NANOSECONDS * DRAM_IO_FREQ / 1000);
  const static uint64_t tCAS = detail::ceil(1.0 * tCAS_DRAM_NANOSECONDS * DRAM_IO_FREQ / 1000);
  const static uint64_t DRAM_DBUS_TURN_AROUND_TIME = detail::ceil(1.0 * DBUS_TURN_AROUND_NANOSECONDS * DRAM_IO_FREQ / 1000);
  const static uint64_t DRAM_DBUS_RETURN_TIME = detail::ceil(1.0 * BLOCK_SIZE / DRAM_CHANNEL_WIDTH);

  std::array<DRAM_CHANNEL, DRAM_CHANNELS> channels;

  MEMORY_CONTROLLER(double freq_scale) : champsim::operable(freq_scale), MemoryRequestConsumer(std::numeric_limits<unsigned>::max()) {}

  int add_rq(PACKET* packet) override;
  int add_wq(PACKET* packet) override;
  int add_pq(PACKET* packet) override;

  void operate() override;

  uint32_t get_occupancy(uint8_t queue_type, uint64_t address) override;
  uint32_t get_size(uint8_t queue_type, uint64_t address) override;

  uint32_t dram_get_channel(uint64_t address);
  uint32_t dram_get_rank(uint64_t address);
  uint32_t dram_get_bank(uint64_t address);
  uint32_t dram_get_row(uint64_t address);
  uint32_t dram_get_column(uint64_t address);
};

#endif
