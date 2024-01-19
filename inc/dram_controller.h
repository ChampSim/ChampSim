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
#include "champsim_constants.h"
#include "channel.h"
#include "chrono.h"
#include "dram_stats.h"
#include "operable.h"


#ifdef RAMULATOR
#include "../ramulator2/src/base/base.h"
#include "../ramulator2/src/base/request.h"
#include "../ramulator2/src/base/config.h"
#include "../ramulator2/src/frontend/frontend.h"
#include "../ramulator2/src/memory_system/memory_system.h"

#include <map>
namespace ramulator
{
  class Request;
  class MemoryBase;
}
#endif

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
  queue_type WQ{DRAM_WQ_SIZE};
  queue_type RQ{DRAM_RQ_SIZE};

  // these values control when to send out a burst of writes
  constexpr static std::size_t DRAM_WRITE_HIGH_WM = ((DRAM_WQ_SIZE * 7) >> 3);         // 7/8th
  constexpr static std::size_t DRAM_WRITE_LOW_WM = ((DRAM_WQ_SIZE * 6) >> 3);          // 6/8th
  constexpr static std::size_t MIN_DRAM_WRITES_PER_SWITCH = ((DRAM_WQ_SIZE * 1) >> 2); // 1/4

  struct BANK_REQUEST {
    bool valid = false;
    bool row_buffer_hit = false;
    bool need_refresh = false;
    bool under_refresh = false;

    std::optional<std::size_t> open_row{};

    champsim::chrono::clock::time_point ready_time{};

    queue_type::iterator pkt;
  };

  using request_array_type = std::array<BANK_REQUEST, DRAM_RANKS * DRAM_BANKS>;
  request_array_type bank_request = {};
  request_array_type::iterator active_request = std::end(bank_request);

  bool write_mode = false;

  std::size_t refresh_row = 0;
  champsim::chrono::clock::time_point last_refresh{};
  champsim::chrono::clock::time_point dbus_cycle_available{};


  using stats_type = dram_stats;
  stats_type roi_stats, sim_stats;

  // Latencies
  const champsim::chrono::clock::duration tRP, tRCD, tCAS, tREF, DRAM_DBUS_TURN_AROUND_TIME, DRAM_DBUS_RETURN_TIME;

  const std::size_t ROWS, COLUMNS, RANKS, BANKS;

  DRAM_CHANNEL(champsim::chrono::picoseconds clock_period_, champsim::chrono::picoseconds t_rp, champsim::chrono::picoseconds t_rcd,
               champsim::chrono::picoseconds t_cas, champsim::chrono::picoseconds turnaround, std::size_t rows, std::size_t columns, std::size_t ranks,
               std::size_t banks);

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

  unsigned long get_rank(champsim::address address) const;
  unsigned long get_bank(champsim::address address) const;
  unsigned long get_row(champsim::address address) const;
  unsigned long get_column(champsim::address address) const;
};


#ifdef RAMULATOR

  namespace Ramulator {

    //here is our frontend type
    class ChampSimRamulator : public IFrontEnd, public Implementation {
    RAMULATOR_REGISTER_IMPLEMENTATION(IFrontEnd, ChampSimRamulator, "ChampSim", "ChampSim frontend.")

    public:
      void init() override { };
      void tick() override { };

      bool receive_external_requests(int req_type_id, Addr_t addr, int source_id, std::function<void(Request&)> callback) override {
        return m_memory_system->send({addr, req_type_id, source_id, callback});
      }

    private:
      bool is_finished() override { return true; };
    };
  }

#endif

class MEMORY_CONTROLLER : public champsim::operable
{
  using channel_type = champsim::channel;
  using request_type = typename channel_type::request_type;
  using response_type = typename channel_type::response_type;
  std::vector<channel_type*> queues;

  void initiate_requests();
  bool add_rq(const request_type& packet, champsim::channel* ul);
  bool add_wq(const request_type& packet);

  #ifdef RAMULATOR
  Ramulator::IFrontEnd* ramulator2_frontend;
  Ramulator::IMemorySystem* ramulator2_memorysystem;

  struct RAMULATOR_Q_ENTRY
  {
    long addr;
    DRAM_CHANNEL::request_type pkt;
  };
  //queue needed to manage packet return requests
  std::vector<RAMULATOR_Q_ENTRY> RAMULATOR_RQ;

  template <typename T>
  ramulator::MemoryBase* create_memory_controller();
  #endif

public:
  std::vector<DRAM_CHANNEL> channels;

  MEMORY_CONTROLLER(champsim::chrono::picoseconds clock_period_, champsim::chrono::picoseconds t_rp, champsim::chrono::picoseconds t_rcd,
                    champsim::chrono::picoseconds t_cas, champsim::chrono::picoseconds turnaround, std::vector<channel_type*>&& ul);

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
