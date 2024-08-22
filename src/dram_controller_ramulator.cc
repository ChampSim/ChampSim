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

#include "dram_controller_ramulator.h"

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
                                     std::size_t columns, std::size_t ranks, std::size_t banks, std::size_t rows_per_refresh, std::string ramulator_config_file)
    : champsim::operable(clock_period_), queues(std::move(ul)), channel_width(chan_width)
{
  //this line can be used to read in the config as a file (this might be easier and more intuitive for users familiar with Ramulator)
  //the full file path should be included, otherwise Ramulator looks in the current working directory (BAD)
  config = Ramulator::Config::parse_config_file(ramulator_config_file, {});

  //force frontend to be champsim, clock ratio == 1, no instruction limit, and no v->p address translation layers
  config["Frontend"]["impl"] = "ChampSim";
  config["Frontend"]["clock_ratio"] = 1;
  config["Frontend"]["num_expected_insts"] = 0;
  config["Frontend"]["Translation"]["impl"] = "None";

  //force memory controller clock scale to 1
  config["MemorySystem"]["clock_ratio"] = 1;

  //create our frontend (us) and the memory system (ramulator)
  ramulator2_frontend = Ramulator::Factory::create_frontend(config);
  ramulator2_memorysystem = Ramulator::Factory::create_memory_system(config);

  //connect the two. we can use this connection to get some more information from ramulator
  ramulator2_frontend->connect_memory_system(ramulator2_memorysystem);
  ramulator2_memorysystem->connect_frontend(ramulator2_frontend);

  //correct clock scale for ramulator2 frequency. Looks like this may point to an inaccuracy in our own model:
  //although the data bus is running at freq f, the memory controller runs at half this (f/2). This is where "DDR" gets its name

  //not sure how to do this any better. I don't like relying on DRAM_IO_FREQ, but we also can't determine this value
  //ahead of time, since it is controlled by the ramulator config file.
  clock_period = clock_period * 2;
}


long MEMORY_CONTROLLER::operate()
{
  long progress{0};

  initiate_requests();

  //tick ramulator.
  //we will assume no deadlock, since there are no ways to measure progress
  ramulator2_memorysystem->tick();
  progress = 1;

  
  return progress;
}

void MEMORY_CONTROLLER::initialize()
{

  //ramulator will print this information out upon startup. We might be able to derive size somehow
  fmt::print("Refer to Ramulator configuration for Off-chip DRAM Size and Configuration\n");
  YAML::Emitter em;
  em << config;
  fmt::print("{}\n",em.c_str());
 
}

void MEMORY_CONTROLLER::begin_phase()
{

}

void MEMORY_CONTROLLER::end_phase(unsigned cpu)
{
  //this happens to also print stats. Finalize for each phase past the warmup
  if(!warmup)
  {
    ramulator2_frontend->finalize();
    ramulator2_memorysystem->finalize();
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

void MEMORY_CONTROLLER::return_packet_rq_rr(Ramulator::Request& req, dram_request_type pkt)
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
      if(packet.response_requested)
      {
        dram_request_type pkt = dram_request_type{packet};
        pkt.to_return = {&ul->returned};
        return ramulator2_frontend->receive_external_requests(int(Ramulator::Request::Type::Read), packet.address.to<int64_t>(), packet.type == access_type::PREFETCH ? 1 : 0, [=](Ramulator::Request& req) {return_packet_rq_rr(req,pkt);});
      }
      else
      {
        //otherwise feed to ramulator directly with no response requested
        return ramulator2_frontend->receive_external_requests(int(Ramulator::Request::Type::Read), packet.address.to<int64_t>(), packet.type == access_type::PREFETCH ? 1 : 0,[this](Ramulator::Request& req){});
      }
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
      return ramulator2_frontend->receive_external_requests(Ramulator::Request::Type::Write, packet.address.to<int64_t>(), 0, [](Ramulator::Request& req){});
    return true;
}

/*
 * | row address | rank index | column address | bank index | channel | block
 * offset |
 */

//These are all inaccurate and will need to be updated when using Ramulator. We can grab some of these values from the config
//others are part of spec that aren't as easily obtained

unsigned long MEMORY_CONTROLLER::dram_get_channel(champsim::address address) const {
  assert(false);
  return(0);
}

unsigned long MEMORY_CONTROLLER::dram_get_bank(champsim::address address) const { 
  assert(false);
  return(0);
}

unsigned long MEMORY_CONTROLLER::dram_get_column(champsim::address address) const {
  assert(false);
  return(0);
}

unsigned long MEMORY_CONTROLLER::dram_get_rank(champsim::address address) const {
  assert(false);
  return(0);
}

unsigned long MEMORY_CONTROLLER::dram_get_row(champsim::address address) const { 
  assert(false);
  return(0);
}

champsim::data::bytes MEMORY_CONTROLLER::size() const
{

  return(champsim::data::bytes(0));
}
// LCOV_EXCL_START Exclude the following function from LCOV
void MEMORY_CONTROLLER::print_deadlock()
{
}
// LCOV_EXCL_STOP
