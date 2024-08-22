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
struct dram_request_type {
    bool scheduled = false;
    bool forward_checked = false;

    uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};

    uint32_t pf_metadata = 0;

    champsim::address address{};
    champsim::address v_address{};
    champsim::address data{};
    champsim::chrono::clock::time_point ready_time = champsim::chrono::clock::time_point::max();

    std::vector<uint64_t> instr_depend_on_me{};
    std::vector<std::deque<champsim::channel::response_type>*> to_return{};

    explicit dram_request_type(const typename champsim::channel::request_type& req);
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

  Ramulator::IFrontEnd* ramulator2_frontend;
  Ramulator::IMemorySystem* ramulator2_memorysystem;
  YAML::Node config;

  void return_packet_rq_rr(Ramulator::Request& req, dram_request_type pkt);

  template <typename T>
  ramulator::MemoryBase* create_memory_controller();


public:
  MEMORY_CONTROLLER(champsim::chrono::picoseconds clock_period_, champsim::chrono::picoseconds t_rp, champsim::chrono::picoseconds t_rcd,
                    champsim::chrono::picoseconds t_cas, champsim::chrono::microseconds refresh_period, champsim::chrono::picoseconds turnaround, std::vector<channel_type*>&& ul, std::size_t rq_size,
                    std::size_t wq_size, std::size_t chans, champsim::data::bytes chan_width, std::size_t rows, std::size_t columns, std::size_t ranks,
                    std::size_t banks, std::size_t rows_per_refresh, std::string ramulator_config_file);


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
