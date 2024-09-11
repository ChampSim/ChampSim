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

#include "dram_controller.h"

#include <algorithm>
#include <cfenv>
#include <cmath>
#include <fmt/core.h>

#include "deadlock.h"
#include "instruction.h"
#include "util/bits.h" // for lg2, bitmask
#include "util/span.h"
#include "util/units.h"

MEMORY_CONTROLLER::MEMORY_CONTROLLER(champsim::chrono::picoseconds clock_period_, champsim::chrono::picoseconds t_rp, champsim::chrono::picoseconds t_rcd,
                                     champsim::chrono::picoseconds t_cas, champsim::chrono::microseconds refresh_period, champsim::chrono::picoseconds turnaround, std::vector<channel_type*>&& ul,
                                     std::size_t rq_size, std::size_t wq_size, std::size_t chans, champsim::data::bytes chan_width, std::size_t rows,
                                     std::size_t columns, std::size_t ranks, std::size_t banks, std::size_t rows_per_refresh, std::string model_config_file)
    : champsim::operable(clock_period_), queues(std::move(ul))
{
  if(model_config_file == "")
    model_config_file = "test/config/ramulator-yaml/ramulator_8GB.yaml";
  //this line can be used to read in the config as a file (this might be easier and more intuitive for users familiar with Ramulator)
  //the full file path should be included, otherwise Ramulator looks in the current working directory (BAD)
  config = Ramulator::Config::parse_config_file(model_config_file, {});

  //force frontend to be champsim, this ensures we are linked properly here
  config["Frontend"]["impl"] = "ChampSim";

  //force memory controller clock scale to 1 (this doesnt do anything as far as I know, but should ensure consistency)
  config["MemorySystem"]["clock_ratio"] = 1;

  //force translation to disabled
  config["Frontend"]["Translation"]["impl"] = "NoTranslation";
  config["Frontend"]["Translation"]["max_addr"] = 0;

  //force ChampSimPlugin to be an active plugin in the dram controller
  YAML::Node active_plugins = config["MemorySystem"]["Controller"]["plugins"];
  bool found_champsim_plugin = false;
  for(auto plugin : active_plugins)
    if(plugin["ControllerPlugin"]["impl"].as<std::string>() == "ChampSimPlugin")
      found_champsim_plugin = true;
  //didn't find active champsim plugin, adding one
  if(!found_champsim_plugin)
  {
    YAML::Node controller_plugin;
    YAML::Node champsim_plugin;
    champsim_plugin["impl"] = "ChampSimPlugin";
    controller_plugin["ControllerPlugin"] = champsim_plugin;
    config["MemorySystem"]["Controller"]["plugins"].push_back(controller_plugin);
  }

  //create our frontend (us) and the memory system (ramulator)
  ramulator2_frontend = Ramulator::Factory::create_frontend(config);
  ramulator2_memorysystem = Ramulator::Factory::create_memory_system(config);

  //connect the two. we can use this connection to get some more information from ramulator
  ramulator2_frontend->connect_memory_system(ramulator2_memorysystem);
  ramulator2_memorysystem->connect_frontend(ramulator2_frontend);

  //correct clock scale for ramulator2 frequency
  clock_period = champsim::chrono::picoseconds(uint64_t(ramulator2_memorysystem->get_tCK() * 1e3));
  //its worth noting here that the rate of calls to ramulator2 should be half of that of champsim's mc model,
  //since Champsim expects a call to the model for every dbus period, and ramulator expects once per memory controller period.

  //grab channel width from Ramulator
  channel_width = champsim::data::bytes(Ramulator::get_ramulator_channel_width(ramulator2_frontend));

  //this will help report stats
  const auto slicer = DRAM_CHANNEL::make_slicer(LOG2_BLOCK_SIZE + champsim::lg2(chans), rows, columns, ranks, banks);
  for (std::size_t i{0}; i < Ramulator::get_ramulator_field_size(ramulator2_frontend,"channel"); ++i) {
    channels.emplace_back(clock_period_, t_rp, t_rcd, t_cas, refresh_period, turnaround, rows_per_refresh, channel_width, rq_size, wq_size, slicer);
    channels[i].ramulator2_frontend = ramulator2_frontend;
  }
}

DRAM_CHANNEL::DRAM_CHANNEL(champsim::chrono::picoseconds clock_period_, champsim::chrono::picoseconds t_rp, champsim::chrono::picoseconds t_rcd,
                           champsim::chrono::picoseconds t_cas, champsim::chrono::microseconds refresh_period, champsim::chrono::picoseconds turnaround, std::size_t rows_per_refresh, 
                           champsim::data::bytes width, std::size_t rq_size, std::size_t wq_size, slicer_type slice)
    : champsim::operable(clock_period_), address_slicer(slice)
{}

auto DRAM_CHANNEL::make_slicer(std::size_t start_pos, std::size_t rows, std::size_t columns, std::size_t ranks, std::size_t banks) -> slicer_type
{
  std::array<std::size_t, slicer_type::size()> params{};
  params.at(SLICER_ROW_IDX) = rows;
  params.at(SLICER_COLUMN_IDX) = columns;
  params.at(SLICER_RANK_IDX) = ranks;
  params.at(SLICER_BANK_IDX) = banks;
  return std::apply([start = start_pos](auto... p) { return champsim::make_contiguous_extent_set(start, champsim::lg2(p)...); }, params);
}

long MEMORY_CONTROLLER::operate()
{
  long progress{0};

  initiate_requests();

  //tick ramulator.
  ramulator2_memorysystem->tick();

  //check for ramulator progress
  progress += Ramulator::get_ramulator_progress(ramulator2_frontend);

  return progress;
}

long DRAM_CHANNEL::operate()
{
  long progress{1};
  return progress;
}

void DRAM_CHANNEL::initialize() {}
void DRAM_CHANNEL::begin_phase() {}

void DRAM_CHANNEL::end_phase(unsigned /*cpu*/) { roi_stats = sim_stats; }


DRAM_CHANNEL::request_type::request_type(const typename champsim::channel::request_type& req)
    : pf_metadata(req.pf_metadata), address(req.address), v_address(req.address), data(req.data), instr_depend_on_me(req.instr_depend_on_me)
{
  asid[0] = req.asid[0];
  asid[1] = req.asid[1];
}

void DRAM_CHANNEL::print_deadlock() {}

void MEMORY_CONTROLLER::initialize()
{
  using namespace champsim::data::data_literals;
  using namespace std::literals::chrono_literals;
  auto sz = this->size();
  if (champsim::data::gibibytes gb_sz{sz}; gb_sz > 1_GiB) {
    fmt::print("Off-chip DRAM Size: {}", gb_sz);
  } else if (champsim::data::mebibytes mb_sz{sz}; mb_sz > 1_MiB) {
    fmt::print("Off-chip DRAM Size: {}", mb_sz);
  } else if (champsim::data::kibibytes kb_sz{sz}; kb_sz > 1_kiB) {
    fmt::print("Off-chip DRAM Size: {}", kb_sz);
  } else {
    fmt::print("Off-chip DRAM Size: {}", sz);
  }
  fmt::print(" Channels: {} Width: {}-bit Data Rate: {} MT/s\n", std::size(channels), champsim::data::bits_per_byte * channel_width.count(),
             1us / (clock_period/2.0));
}

void MEMORY_CONTROLLER::begin_phase()
{
  std::size_t chan_idx = 0;
  for (auto& chan : channels) {
    DRAM_CHANNEL::stats_type new_stats;
    new_stats.name = "Channel " + std::to_string(chan_idx++);
    chan.sim_stats = new_stats;
    chan.warmup = warmup;
  }

  for (auto* ul : queues) {
    channel_type::stats_type ul_new_roi_stats;
    channel_type::stats_type ul_new_sim_stats;
    ul->roi_stats = ul_new_roi_stats;
    ul->sim_stats = ul_new_sim_stats;
  }  
}

void MEMORY_CONTROLLER::end_phase(unsigned cpu)
{
   //finalize ramulator (if not warmup)
  if(!warmup)
  {
    ramulator2_frontend->finalize();
    ramulator2_memorysystem->finalize();
  }

  //grab stats from each channel for ramulator
  for(size_t i = 0; i < channels.size(); i++)
  {
    channels[i].sim_stats.dbus_cycle_congested = (long)Ramulator::get_ramulator_stat(ramulator2_frontend,"DBUS_CYCLE_CONGESTED",i);
    channels[i].sim_stats.dbus_count_congested = (uint64_t)Ramulator::get_ramulator_stat(ramulator2_frontend,"DBUS_COUNT_CONGESTED",i);
    channels[i].sim_stats.refresh_cycles       = (uint64_t)Ramulator::get_ramulator_stat(ramulator2_frontend,"REFRESH_CYCLES",i);
    channels[i].sim_stats.WQ_ROW_BUFFER_HIT    = (unsigned)Ramulator::get_ramulator_stat(ramulator2_frontend,"WQ_ROW_BUFFER_HIT",i);
    channels[i].sim_stats.WQ_ROW_BUFFER_MISS   = (unsigned)Ramulator::get_ramulator_stat(ramulator2_frontend,"WQ_ROW_BUFFER_MISS",i);
    channels[i].sim_stats.RQ_ROW_BUFFER_HIT    = (unsigned)Ramulator::get_ramulator_stat(ramulator2_frontend,"RQ_ROW_BUFFER_HIT",i);
    channels[i].sim_stats.RQ_ROW_BUFFER_MISS   = (unsigned)Ramulator::get_ramulator_stat(ramulator2_frontend,"RQ_ROW_BUFFER_MISS",i);
  }

  //end phase for channels (update stats)
  for (auto& chan : channels) {
    chan.end_phase(cpu);
  }
}

void MEMORY_CONTROLLER::initiate_requests()
{
  // Initiate read requests
  for (auto* ul : queues) {
    for (auto q : {std::ref(ul->RQ), std::ref(ul->PQ)}) {
      auto [begin, end] = champsim::get_span_p(std::cbegin(q.get()), std::cend(q.get()), [ul, this](const auto& pkt) { return this->add_rq(pkt, ul); });
      q.get().erase(begin, end);
    }

    // Initiate write requests
    auto [wq_begin, wq_end] = champsim::get_span_p(std::cbegin(ul->WQ), std::cend(ul->WQ), [this](const auto& pkt) { return this->add_wq(pkt); });
    ul->WQ.erase(wq_begin, wq_end);
  }
}

void MEMORY_CONTROLLER::return_packet_rq_rr(Ramulator::Request& req, DRAM_CHANNEL::request_type pkt)
{
  response_type response{pkt.address, pkt.v_address, pkt.data,
                        pkt.pf_metadata, pkt.instr_depend_on_me};

  for (auto* ret : pkt.to_return) {
    ret->push_back(response);
  }
  return;
};



bool MEMORY_CONTROLLER::add_rq(const request_type& packet, champsim::channel* ul)
{
    //if packet needs response, we need to track its data to return later
    if(!warmup)
    {
      //if not warmup
      bool success;
      if(packet.response_requested)
      {
        DRAM_CHANNEL::request_type pkt = DRAM_CHANNEL::request_type{packet};
        pkt.to_return = {&ul->returned};
        success = ramulator2_frontend->receive_external_requests(int(Ramulator::Request::Type::Read), packet.address.to<int64_t>(), packet.cpu, [=](Ramulator::Request& req) {return_packet_rq_rr(req,pkt);});
      }
      else
      {
        //otherwise feed to ramulator directly with no response requested
        success = ramulator2_frontend->receive_external_requests(int(Ramulator::Request::Type::Read), packet.address.to<int64_t>(), packet.cpu,[](Ramulator::Request& req){});
      }
      return(success);
    }
    else
    {
      //if warmup, just return true and send necessary responses
      if(packet.response_requested)
      {
          response_type response{packet.address, packet.v_address, packet.data,
                                packet.pf_metadata, packet.instr_depend_on_me};
          for (auto* ret : {&ul->returned}) {
            ret->push_back(response);
          }
      }
      return true;
    }
}

bool MEMORY_CONTROLLER::add_wq(const request_type& packet)
{
    //if ramulator, feed directly. Since its a write, no response is needed
    if(!warmup)
    {
      bool success = ramulator2_frontend->receive_external_requests(Ramulator::Request::Type::Write, packet.address.to<int64_t>(), packet.cpu, [](Ramulator::Request& req){});
      if(!success)
        ++channels[dram_get_channel(packet.address)].sim_stats.WQ_FULL;
    }
    return true;
}

//grab address mapping data from ramulator
unsigned long MEMORY_CONTROLLER::dram_get_channel(champsim::address address) const {
  return(Ramulator::translate_to_ramulator_addr_field(ramulator2_frontend,"channel",address.to<int64_t>()));
}

unsigned long MEMORY_CONTROLLER::dram_get_bank(champsim::address address) const {
  return channels.at(dram_get_channel(address)).get_bank(address);
}

unsigned long MEMORY_CONTROLLER::dram_get_column(champsim::address address) const {
  return channels.at(dram_get_channel(address)).get_column(address);
}

unsigned long MEMORY_CONTROLLER::dram_get_rank(champsim::address address) const {
  return channels.at(dram_get_channel(address)).get_rank(address);
}

unsigned long MEMORY_CONTROLLER::dram_get_row(champsim::address address) const { 
  return channels.at(dram_get_channel(address)).get_row(address);
}

unsigned long DRAM_CHANNEL::get_bank(champsim::address address) const 
{
  return(Ramulator::translate_to_ramulator_addr_field(ramulator2_frontend,"bank",address.to<int64_t>()));
}

unsigned long DRAM_CHANNEL::get_column(champsim::address address) const 
{ 
  return(Ramulator::translate_to_ramulator_addr_field(ramulator2_frontend,"column",address.to<int64_t>()));
}

unsigned long DRAM_CHANNEL::get_rank(champsim::address address) const 
{
  return(Ramulator::translate_to_ramulator_addr_field(ramulator2_frontend,"rank",address.to<int64_t>()));
}

unsigned long DRAM_CHANNEL::get_row(champsim::address address) const 
{
  return(Ramulator::translate_to_ramulator_addr_field(ramulator2_frontend,"row",address.to<int64_t>()));
}

champsim::data::bytes MEMORY_CONTROLLER::size() const
{
 return std::accumulate(std::cbegin(channels), std::cend(channels), champsim::data::bytes{}, [](auto acc, const auto& x) { return acc + x.size(); });
}

// LCOV_EXCL_START Exclude the following function from LCOV
void MEMORY_CONTROLLER::print_deadlock()
{
  //we can't know the contents of the queue in ramulator, so theres nothing to print here
  fmt::print("Ramulator has deadlocked\n");
}
// LCOV_EXCL_STOP

champsim::data::bytes DRAM_CHANNEL::size() const { return(champsim::data::bytes(Ramulator::get_ramulator_size(ramulator2_frontend,0))); }

std::size_t DRAM_CHANNEL::rows() const { return Ramulator::get_ramulator_field_size(ramulator2_frontend,"row"); }
std::size_t DRAM_CHANNEL::columns() const { return Ramulator::get_ramulator_field_size(ramulator2_frontend,"column"); }
std::size_t DRAM_CHANNEL::ranks() const { return Ramulator::get_ramulator_field_size(ramulator2_frontend,"rank"); }
std::size_t DRAM_CHANNEL::banks() const { return Ramulator::get_ramulator_field_size(ramulator2_frontend,"bank"); }
std::size_t DRAM_CHANNEL::bank_request_capacity() const { return ranks() * banks(); }