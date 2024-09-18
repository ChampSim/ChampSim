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
#include <cstddef>  // for size_t
#include <cstdint>  // for uint64_t, uint32_t, uint8_t
#include <deque>    // for deque
#include <iterator> // for end
#include <limits>
#include <optional>
#include <string>

#include "address.h"
#include "channel.h"
#include "chrono.h"
#include "dram_stats.h"
#include "extent_set.h"
#include "operable.h"

struct DRAM_CHANNEL final : public champsim::operable {
  using response_type = typename champsim::channel::response_type;
  struct request_type {
    bool scheduled = false;
    bool forward_checked = false;

    uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};

    uint32_t pf_metadata = 0;

    champsim::address address{};
    champsim::address v_address{};
    champsim::address data{};
    champsim::chrono::clock::time_point ready_time = champsim::chrono::clock::time_point::max();

    std::vector<uint64_t> instr_depend_on_me{};
    std::vector<std::deque<response_type>*> to_return{};

    explicit request_type(const typename champsim::channel::request_type& req);
  };
  using value_type = request_type;
  using queue_type = std::vector<std::optional<value_type>>;
  queue_type WQ;
  queue_type RQ;
  queue_type PQ;

  /*
   * | row address | rank index | column address | bank index | channel | block
   * offset |
   */
  constexpr static std::size_t SLICER_ROW_IDX = 3;
  constexpr static std::size_t SLICER_COLUMN_IDX = 1;
  constexpr static std::size_t SLICER_RANK_IDX = 2;
  constexpr static std::size_t SLICER_BANK_IDX = 0;
  using slicer_type = champsim::extent_set<champsim::dynamic_extent, champsim::dynamic_extent, champsim::dynamic_extent, champsim::dynamic_extent>;
  const slicer_type address_slicer;

  struct BANK_REQUEST {
    bool valid = false, row_buffer_hit = false;

    std::optional<std::size_t> open_row{};

    champsim::chrono::clock::time_point ready_time{};

    queue_type::iterator pkt;
  };

  using request_array_type = std::vector<BANK_REQUEST>;
  request_array_type bank_request{ranks() * banks()};
  request_array_type::iterator active_request = std::end(bank_request);

  std::size_t bank_request_index(champsim::address addr) const;

  bool write_mode = false;
  champsim::chrono::clock::time_point dbus_cycle_available{};

  using stats_type = dram_stats;
  stats_type roi_stats, sim_stats;

  const champsim::data::bytes channel_width;
  // Latencies
  const champsim::chrono::clock::duration tRP, tRCD, tCAS, DRAM_DBUS_TURN_AROUND_TIME, DRAM_DBUS_RETURN_TIME;

  DRAM_CHANNEL(champsim::chrono::picoseconds clock_period_, champsim::chrono::picoseconds t_rp, champsim::chrono::picoseconds t_rcd,
               champsim::chrono::picoseconds t_cas, champsim::chrono::picoseconds turnaround, champsim::data::bytes width, std::size_t rq_size,
               std::size_t wq_size, slicer_type slice);

  void check_write_collision();
  void check_read_collision();
  long finish_dbus_request();
  void swap_write_mode();
  long populate_dbus();
  long schedule_packets();

  void initialize() final;
  long operate() final;
  void begin_phase() final;
  void end_phase(unsigned cpu) final;
  void print_deadlock() final;

  [[nodiscard]] champsim::data::bytes size() const;

  unsigned long get_rank(champsim::address address) const;
  unsigned long get_bank(champsim::address address) const;
  unsigned long get_row(champsim::address address) const;
  unsigned long get_column(champsim::address address) const;

  std::size_t rows() const;
  std::size_t columns() const;
  std::size_t ranks() const;
  std::size_t banks() const;
  std::size_t bank_request_capacity() const;
  static slicer_type make_slicer(std::size_t start_pos, std::size_t rows, std::size_t columns, std::size_t ranks, std::size_t banks);
};

class MEMORY_CONTROLLER : public champsim::operable
{
  using channel_type = champsim::channel;
  using request_type = typename channel_type::request_type;
  using response_type = typename channel_type::response_type;
  std::vector<channel_type*> queues;
  const champsim::data::bytes channel_width;

  void initiate_requests();
  bool add_rq(const request_type& packet, champsim::channel* ul);
  bool add_wq(const request_type& packet);
  bool add_pq(const request_type& packet, champsim::channel* ul);

public:
  std::vector<DRAM_CHANNEL> channels;

  MEMORY_CONTROLLER(champsim::chrono::picoseconds clock_period_, champsim::chrono::picoseconds t_rp, champsim::chrono::picoseconds t_rcd,
                    champsim::chrono::picoseconds t_cas, champsim::chrono::picoseconds turnaround, std::vector<channel_type*>&& ul, std::size_t rq_size,
                    std::size_t wq_size, std::size_t chans, champsim::data::bytes chan_width, std::size_t rows, std::size_t columns, std::size_t ranks,
                    std::size_t banks);

  void initialize() final;
  long operate() final;
  void begin_phase() final;
  void end_phase(unsigned cpu) final;
  void print_deadlock() final;

  [[nodiscard]] champsim::data::bytes size() const;

  unsigned long dram_get_channel(champsim::address address) const;
  unsigned long dram_get_rank(champsim::address address) const;
  unsigned long dram_get_bank(champsim::address address) const;
  unsigned long dram_get_row(champsim::address address) const;
  unsigned long dram_get_column(champsim::address address) const;
};

#endif
