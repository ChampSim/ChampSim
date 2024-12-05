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

struct DRAM_ADDRESS_MAPPING {
  constexpr static std::size_t SLICER_OFFSET_IDX = 0;
  constexpr static std::size_t SLICER_CHANNEL_IDX = 1;
  constexpr static std::size_t SLICER_BANKGROUP_IDX = 2;
  constexpr static std::size_t SLICER_BANK_IDX = 3;
  constexpr static std::size_t SLICER_COLUMN_IDX = 4;
  constexpr static std::size_t SLICER_RANK_IDX = 5;
  constexpr static std::size_t SLICER_ROW_IDX = 6;

  using slicer_type = champsim::extent_set<champsim::dynamic_extent, champsim::dynamic_extent, champsim::dynamic_extent, champsim::dynamic_extent,
                                           champsim::dynamic_extent, champsim::dynamic_extent, champsim::dynamic_extent>;
  const slicer_type address_slicer;

  const std::size_t prefetch_size;

  DRAM_ADDRESS_MAPPING(champsim::data::bytes channel_width, std::size_t pref_size, std::size_t channels, std::size_t bankgroups, std::size_t banks,
                       std::size_t columns, std::size_t ranks, std::size_t rows);
  static slicer_type make_slicer(champsim::data::bytes channel_width, std::size_t pref_size, std::size_t channels, std::size_t bankgroups, std::size_t banks,
                                 std::size_t columns, std::size_t ranks, std::size_t rows);

  unsigned long get_channel(champsim::address address) const;
  unsigned long get_rank(champsim::address address) const;
  unsigned long get_bankgroup(champsim::address address) const;
  unsigned long get_bank(champsim::address address) const;
  unsigned long get_row(champsim::address address) const;
  unsigned long get_column(champsim::address address) const;

  /**
   * Perform the hashing operations for indexing our channels, banks, and bankgroups.
   * This is done to increase parallelism when serving requests at the DRAM level.
   *
   * :param address: The physical address at which the hashing operation is occurring.
   * :param segment_size: The number of row bits extracted during each iteration. (# of row bits / segment_size) == # of XOR operations
   * :param segment_offset: The bit offset within the segment that the XOR operation will occur at. The bits taken from the segment will be [segment_offset +
   * field_bits : segment_offset]
   *
   * :param field: The input index that is being permuted by the operation.
   * :param field_bits: The length of the index in bits.
   *
   * Each iteration of the operation takes the selected bits of the segment and XORs them with the entirety of the field
   * which should be equal or greater than length (in the case of the last iteration). This continues until no bits remain
   * within the row that have not been XOR'd with the field.
   */
  unsigned long swizzle_bits(champsim::address address, unsigned long segment_size, champsim::data::bits segment_offset, unsigned long field,
                             unsigned long field_bits) const;

  bool is_collision(champsim::address a, champsim::address b) const;

  std::size_t rows() const;
  std::size_t columns() const;
  std::size_t ranks() const;
  std::size_t bankgroups() const;
  std::size_t banks() const;
  std::size_t channels() const;
};

struct DRAM_CHANNEL final : public champsim::operable {
  using response_type = typename champsim::channel::response_type;

  const DRAM_ADDRESS_MAPPING address_mapping;

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

  /*
   * | row address | rank index | column address | bank index | channel | block
   * offset |
   */

  struct BANK_REQUEST {
    bool valid = false, row_buffer_hit = false, need_refresh = false, under_refresh = false;

    std::optional<std::size_t> open_row{};

    champsim::chrono::clock::time_point ready_time{};

    queue_type::iterator pkt;
  };

  const champsim::data::bytes channel_width;

  using request_array_type = std::vector<BANK_REQUEST>;
  request_array_type bank_request;
  request_array_type::iterator active_request;

  // track bankgroup accesses
  std::vector<champsim::chrono::clock::time_point> bankgroup_readytime{address_mapping.ranks() * address_mapping.bankgroups(),
                                                                       champsim::chrono::clock::time_point{}};

  std::size_t bank_request_index(champsim::address addr) const;
  std::size_t bankgroup_request_index(champsim::address addr) const;

  bool write_mode = false;
  champsim::chrono::clock::time_point dbus_cycle_available{};

  std::size_t refresh_row = 0;
  champsim::chrono::clock::time_point last_refresh{};
  std::size_t DRAM_ROWS_PER_REFRESH;

  using stats_type = dram_stats;
  stats_type roi_stats, sim_stats;

  // Latencies
  const champsim::chrono::clock::duration tRP, tRCD, tCAS, tRAS, tREF, tRFC, DRAM_DBUS_TURN_AROUND_TIME, DRAM_DBUS_RETURN_TIME, DRAM_DBUS_BANKGROUP_STALL;

  // data bus period
  champsim::chrono::picoseconds data_bus_period{};

  DRAM_CHANNEL(champsim::chrono::picoseconds dbus_period, champsim::chrono::picoseconds mc_period, std::size_t t_rp, std::size_t t_rcd, std::size_t t_cas,
               std::size_t t_ras, champsim::chrono::microseconds refresh_period, std::size_t refreshes_per_period, champsim::data::bytes width,
               std::size_t rq_size, std::size_t wq_size, DRAM_ADDRESS_MAPPING addr_mapping);

  void check_write_collision();
  void check_read_collision();
  long finish_dbus_request();
  long schedule_refresh();
  void swap_write_mode();
  long populate_dbus();
  DRAM_CHANNEL::queue_type::iterator schedule_packet();
  long service_packet(DRAM_CHANNEL::queue_type::iterator pkt);

  void initialize() final;
  long operate() final;
  void begin_phase() final;
  void end_phase(unsigned cpu) final;
  void print_deadlock() final;

  std::size_t bank_request_capacity() const;
  std::size_t bankgroup_request_capacity() const;
  [[nodiscard]] champsim::data::bytes density() const;
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

  const DRAM_ADDRESS_MAPPING address_mapping;

  // data bus period
  champsim::chrono::picoseconds data_bus_period{};

public:
  std::vector<DRAM_CHANNEL> channels;

  MEMORY_CONTROLLER(champsim::chrono::picoseconds dbus_period, champsim::chrono::picoseconds mc_period, std::size_t t_rp, std::size_t t_rcd, std::size_t t_cas,
                    std::size_t t_ras, champsim::chrono::microseconds refresh_period, std::vector<channel_type*>&& ul, std::size_t rq_size, std::size_t wq_size,
                    std::size_t chans, champsim::data::bytes chan_width, std::size_t rows, std::size_t columns, std::size_t ranks, std::size_t bankgroups,
                    std::size_t banks, std::size_t refreshes_per_period);

  void initialize() final;
  long operate() final;
  void begin_phase() final;
  void end_phase(unsigned cpu) final;
  void print_deadlock() final;

  [[nodiscard]] champsim::data::bytes size() const;
};

#endif
