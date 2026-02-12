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
#include "../ramulator2/src/translation/translation.h"

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

ITranslation* m_translation;
public:
    void init() override { m_translation = create_child_ifce<ITranslation>(); };
    void tick() override { };

    bool receive_external_requests(int req_type_id, Addr_t addr, int source_id, std::function<void(Request&)> callback) override {
    return m_memory_system->send({addr, req_type_id, source_id, callback});
    }

private:
    bool is_finished() override { return true; };
};

double get_ramulator_stat(Ramulator::IFrontEnd*, std::string stat_name, size_t channel_no);
size_t translate_to_ramulator_addr_field(Ramulator::IFrontEnd*, std::string field, int64_t addr);
size_t get_ramulator_field_size(Ramulator::IFrontEnd*, std::string field);
long get_ramulator_progress(Ramulator::IFrontEnd*);
uint64_t get_ramulator_size(Ramulator::IFrontEnd*, size_t channel_no);
uint64_t get_ramulator_channel_width(Ramulator::IFrontEnd*);

}

struct DRAM_ADDRESS_MAPPING {

  DRAM_ADDRESS_MAPPING(champsim::data::bytes channel_width, std::size_t pref_size, std::size_t channels, std::size_t bankgroups, std::size_t banks,
                       std::size_t columns, std::size_t ranks, std::size_t rows);

    std::size_t rows() const;
    std::size_t columns() const;
    std::size_t ranks() const;
    std::size_t bankgroups() const;
    std::size_t banks() const;
    std::size_t channels() const;

    unsigned long get_channel(champsim::address address) const {return 0;};
    unsigned long get_rank(champsim::address address) const {return 0;};
    unsigned long get_bankgroup(champsim::address address) const {return 0;};
    unsigned long get_bank(champsim::address address) const {return 0;};
    unsigned long get_row(champsim::address address) const {return 0;};
    unsigned long get_column(champsim::address address) const {return 0;};
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

  Ramulator::IFrontEnd* ramulator2_frontend;

  using stats_type = dram_stats;
  stats_type roi_stats, sim_stats;


  champsim::data::bytes channel_width;

  unsigned long get_rank(champsim::address address) const;
  unsigned long get_bank(champsim::address address) const;
  unsigned long get_row(champsim::address address) const;
  unsigned long get_column(champsim::address address) const;

  [[nodiscard]] champsim::data::bytes size() const;

  std::size_t rows() const;
  std::size_t columns() const;
  std::size_t ranks() const;
  std::size_t banks() const;
  std::size_t bank_request_capacity() const;


  DRAM_CHANNEL(champsim::chrono::picoseconds dbus_period, champsim::chrono::picoseconds mc_period, std::size_t t_rp, std::size_t t_rcd, std::size_t t_cas,
               std::size_t t_ras, champsim::chrono::microseconds refresh_period, std::size_t refreshes_per_period, champsim::data::bytes width,
               std::size_t rq_size, std::size_t wq_size, DRAM_ADDRESS_MAPPING addr_mapping);

  void initialize() final;
  long operate() final;
  void begin_phase() final;
  void end_phase(unsigned cpu) final;
  void print_deadlock() final;

};

class MEMORY_CONTROLLER : public champsim::operable
{
  using channel_type = champsim::channel;
  using request_type = typename channel_type::request_type;
  using response_type = typename channel_type::response_type;
  std::vector<channel_type*> queues;
  champsim::data::bytes channel_width;
    // data bus period
  champsim::chrono::picoseconds data_bus_period{};
  void initiate_requests();
  bool add_rq(const request_type& packet, champsim::channel* ul);
  bool add_wq(const request_type& packet);
  const DRAM_ADDRESS_MAPPING address_mapping;
  Ramulator::IFrontEnd* ramulator2_frontend;
  Ramulator::IMemorySystem* ramulator2_memorysystem;
  YAML::Node config;

  void return_packet_rq_rr(Ramulator::Request& req, DRAM_CHANNEL::request_type pkt);
  


public:
  std::vector<DRAM_CHANNEL> channels;
  
  MEMORY_CONTROLLER(champsim::chrono::picoseconds dbus_period, champsim::chrono::picoseconds mc_period, std::size_t t_rp, std::size_t t_rcd, std::size_t t_cas,
                    std::size_t t_ras, champsim::chrono::microseconds refresh_period, std::vector<channel_type*>&& ul, std::size_t rq_size, std::size_t wq_size,
                    std::size_t chans, champsim::data::bytes chan_width, std::size_t rows, std::size_t columns, std::size_t ranks, std::size_t bankgroups,
                    std::size_t banks, std::size_t refreshes_per_period, std::string model_config_file = "");


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
