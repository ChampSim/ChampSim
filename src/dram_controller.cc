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

#include "champsim_constants.h"
#include "instruction.h"
#include "util.h"

uint64_t cycles(double time, int io_freq)
{
  std::fesetround(FE_UPWARD);
  auto result = std::lrint(time * io_freq);
  return result < 0 ? 0 : static_cast<uint64_t>(result);
}

MEMORY_CONTROLLER::MEMORY_CONTROLLER(double freq_scale, int io_freq, double t_rp, double t_rcd, double t_cas, double turnaround)
    : champsim::operable(freq_scale), tRP(cycles(t_rp / 1000, io_freq)), tRCD(cycles(t_rcd / 1000, io_freq)), tCAS(cycles(t_cas / 1000, io_freq)),
      DRAM_DBUS_TURN_AROUND_TIME(cycles(turnaround / 1000, io_freq)), DRAM_DBUS_RETURN_TIME(cycles(std::ceil(BLOCK_SIZE) / std::ceil(DRAM_CHANNEL_WIDTH), 1))
{
}

void MEMORY_CONTROLLER::operate()
{
  for (auto& channel : channels) {
    if (warmup) {
      for (auto& entry : channel.RQ) {
        for (auto ret : entry.to_return)
          ret->return_data(entry);

        entry = {};
      }

      for (auto& entry : channel.WQ)
        entry = {};
    }

    // Check for forwarding
    channel.check_collision();

    // Finish request
    if (channel.active_request != std::end(channel.bank_request) && channel.active_request->event_cycle <= current_cycle) {
      for (auto ret : channel.active_request->pkt->to_return)
        ret->return_data(*channel.active_request->pkt);

      channel.active_request->valid = false;

      *channel.active_request->pkt = {};
      channel.active_request = std::end(channel.bank_request);
    }

    // Check queue occupancy
    auto wq_occu = static_cast<std::size_t>(std::count_if(std::begin(channel.WQ), std::end(channel.WQ), [](const auto& x) { return x.address != 0; }));
    auto rq_occu = static_cast<std::size_t>(std::count_if(std::begin(channel.RQ), std::end(channel.RQ), [](const auto& x) { return x.address != 0; }));

    // Change modes if the queues are unbalanced
    if ((!channel.write_mode && (wq_occu >= DRAM_WRITE_HIGH_WM || (rq_occu == 0 && wq_occu > 0)))
        || (channel.write_mode && (wq_occu == 0 || (rq_occu > 0 && wq_occu < DRAM_WRITE_LOW_WM)))) {
      // Reset scheduled requests
      for (auto it = std::begin(channel.bank_request); it != std::end(channel.bank_request); ++it) {
        // Leave active request on the data bus
        if (it != channel.active_request && it->valid) {
          // Leave rows charged
          if (it->event_cycle < (current_cycle + tCAS))
            it->open_row = UINT32_MAX;

          // This bank is ready for another DRAM request
          it->valid = false;
          it->pkt->scheduled = false;
          it->pkt->event_cycle = current_cycle;
        }
      }

      // Add data bus turn-around time
      if (channel.active_request != std::end(channel.bank_request))
        channel.dbus_cycle_available = channel.active_request->event_cycle + DRAM_DBUS_TURN_AROUND_TIME; // After ongoing finish
      else
        channel.dbus_cycle_available = current_cycle + DRAM_DBUS_TURN_AROUND_TIME;

      // Invert the mode
      channel.write_mode = !channel.write_mode;
    }

    // Look for requests to put on the bus
    auto iter_next_process = std::min_element(std::begin(channel.bank_request), std::end(channel.bank_request),
                                              [](const auto& lhs, const auto& rhs) { return !rhs.valid || (lhs.valid && lhs.event_cycle < rhs.event_cycle); });
    if (iter_next_process->valid && iter_next_process->event_cycle <= current_cycle) {
      if (channel.active_request == std::end(channel.bank_request) && channel.dbus_cycle_available <= current_cycle) {
        // Bus is available
        // Put this request on the data bus
        channel.active_request = iter_next_process;
        channel.active_request->event_cycle = current_cycle + DRAM_DBUS_RETURN_TIME;

        if (iter_next_process->row_buffer_hit)
          if (channel.write_mode)
            channel.sim_stats.back().WQ_ROW_BUFFER_HIT++;
          else
            channel.sim_stats.back().RQ_ROW_BUFFER_HIT++;
        else if (channel.write_mode)
          channel.sim_stats.back().WQ_ROW_BUFFER_MISS++;
        else
          channel.sim_stats.back().RQ_ROW_BUFFER_MISS++;
      } else {
        // Bus is congested
        if (channel.active_request != std::end(channel.bank_request))
          channel.sim_stats.back().dbus_cycle_congested += (channel.active_request->event_cycle - current_cycle);
        else
          channel.sim_stats.back().dbus_cycle_congested += (channel.dbus_cycle_available - current_cycle);
        channel.sim_stats.back().dbus_count_congested++;
      }
    }

    // Look for queued packets that have not been scheduled
    auto next_schedule = [](const auto& lhs, const auto& rhs) {
      return !(rhs.address != 0 && !rhs.scheduled) || ((lhs.address != 0 && !lhs.scheduled) && lhs.event_cycle < rhs.event_cycle);
    };
    std::vector<PACKET>::iterator iter_next_schedule;
    if (channel.write_mode)
      iter_next_schedule = std::min_element(std::begin(channel.WQ), std::end(channel.WQ), next_schedule);
    else
      iter_next_schedule = std::min_element(std::begin(channel.RQ), std::end(channel.RQ), next_schedule);

    if (is_valid<PACKET>()(*iter_next_schedule) && iter_next_schedule->event_cycle <= current_cycle) {
      uint32_t op_rank = dram_get_rank(iter_next_schedule->address), op_bank = dram_get_bank(iter_next_schedule->address),
               op_row = dram_get_row(iter_next_schedule->address);

      auto op_idx = op_rank * DRAM_BANKS + op_bank;

      if (!channel.bank_request[op_idx].valid) {
        bool row_buffer_hit = (channel.bank_request[op_idx].open_row == op_row);

        // this bank is now busy
        channel.bank_request[op_idx] = {true, row_buffer_hit, op_row, current_cycle + tCAS + (row_buffer_hit ? 0 : tRP + tRCD), iter_next_schedule};

        iter_next_schedule->scheduled = true;
        iter_next_schedule->event_cycle = std::numeric_limits<uint64_t>::max();
      }
    }
  }
}

void MEMORY_CONTROLLER::initialize()
{
  long long int dram_size = DRAM_CHANNELS * DRAM_RANKS * DRAM_BANKS * DRAM_ROWS * DRAM_COLUMNS * BLOCK_SIZE / 1024 / 1024; // in MiB
  std::cout << "Off-chip DRAM Size: ";
  if (dram_size > 1024)
    std::cout << dram_size / 1024 << " GiB";
  else
    std::cout << dram_size << " MiB";
  std::cout << " Channels: " << DRAM_CHANNELS << " Width: " << 8 * DRAM_CHANNEL_WIDTH << "-bit Data Rate: " << DRAM_IO_FREQ << " MT/s" << std::endl;
}

void MEMORY_CONTROLLER::begin_phase()
{
  std::size_t chan_idx = 0;
  for (auto& chan : channels) {
    chan.sim_stats.emplace_back();
    chan.sim_stats.back().name = "Channel " + std::to_string(chan_idx++);
  }
}

void MEMORY_CONTROLLER::end_phase(unsigned)
{
  for (auto& chan : channels)
    chan.roi_stats.push_back(chan.sim_stats.back());
}

void DRAM_CHANNEL::check_collision()
{
  for (auto wq_it = std::begin(WQ); wq_it != std::end(WQ); ++wq_it) {
    if (is_valid<PACKET>{}(*wq_it) && !wq_it->forward_checked) {
      eq_addr<PACKET> checker{wq_it->address, LOG2_BLOCK_SIZE};
      if (auto found = std::find_if(std::begin(WQ), wq_it, checker); found != wq_it) { // Forward check
        *wq_it = {};
      } else if (found = std::find_if(std::next(wq_it), std::end(WQ), checker); found != std::end(WQ)) { // Backward check
        *wq_it = {};
      } else {
        wq_it->forward_checked = true;
      }
    }
  }

  for (auto rq_it = std::begin(RQ); rq_it != std::end(RQ); ++rq_it) {
    if (is_valid<PACKET>{}(*rq_it) && !rq_it->forward_checked) {
      eq_addr<PACKET> checker{rq_it->address, LOG2_BLOCK_SIZE};
      if (auto wq_it = std::find_if(std::begin(WQ), std::end(WQ), checker); wq_it != std::end(WQ)) {
        rq_it->data = wq_it->data;
        for (auto ret : rq_it->to_return)
          ret->return_data(*rq_it);

        *rq_it = {};
      } else if (auto found = std::find_if(std::begin(RQ), rq_it, checker); found != rq_it) {
        auto instr_copy = std::move(found->instr_depend_on_me);
        auto ret_copy = std::move(found->to_return);

        std::set_union(std::begin(instr_copy), std::end(instr_copy), std::begin(rq_it->instr_depend_on_me), std::end(rq_it->instr_depend_on_me),
                       std::back_inserter(found->instr_depend_on_me), ooo_model_instr::program_order);
        std::set_union(std::begin(ret_copy), std::end(ret_copy), std::begin(rq_it->to_return), std::end(rq_it->to_return),
                       std::back_inserter(found->to_return));

        *rq_it = {};
      } else if (found = std::find_if(std::next(rq_it), std::end(RQ), checker); found != std::end(RQ)) {
        auto instr_copy = std::move(found->instr_depend_on_me);
        auto ret_copy = std::move(found->to_return);

        std::set_union(std::begin(instr_copy), std::end(instr_copy), std::begin(rq_it->instr_depend_on_me), std::end(rq_it->instr_depend_on_me),
                       std::back_inserter(found->instr_depend_on_me), ooo_model_instr::program_order);
        std::set_union(std::begin(ret_copy), std::end(ret_copy), std::begin(rq_it->to_return), std::end(rq_it->to_return),
                       std::back_inserter(found->to_return));

        *rq_it = {};
      } else {
        rq_it->forward_checked = true;
      }
    }
  }
}

bool MEMORY_CONTROLLER::add_rq(const PACKET& packet)
{
  auto& channel = channels[dram_get_channel(packet.address)];

  // Find empty slot
  if (auto rq_it = std::find_if_not(std::begin(channel.RQ), std::end(channel.RQ), is_valid<PACKET>()); rq_it != std::end(channel.RQ)) {
    *rq_it = packet;
    rq_it->forward_checked = false;
    rq_it->event_cycle = current_cycle;

    return true;
  }

  return false;
}

bool MEMORY_CONTROLLER::add_wq(const PACKET& packet)
{
  auto& channel = channels[dram_get_channel(packet.address)];

  // search for the empty index
  if (auto wq_it = std::find_if_not(std::begin(channel.WQ), std::end(channel.WQ), is_valid<PACKET>()); wq_it != std::end(channel.WQ)) {
    *wq_it = packet;
    wq_it->forward_checked = false;
    wq_it->event_cycle = current_cycle;

    return true;
  }

  channel.sim_stats.back().WQ_FULL++;
  return false;
}

bool MEMORY_CONTROLLER::add_pq(const PACKET& packet) { return add_rq(packet); }

/*
 * | row address | rank index | column address | bank index | channel | block
 * offset |
 */

uint32_t MEMORY_CONTROLLER::dram_get_channel(uint64_t address)
{
  int shift = LOG2_BLOCK_SIZE;
  return (address >> shift) & champsim::bitmask(champsim::lg2(DRAM_CHANNELS));
}

uint32_t MEMORY_CONTROLLER::dram_get_bank(uint64_t address)
{
  int shift = champsim::lg2(DRAM_CHANNELS) + LOG2_BLOCK_SIZE;
  return (address >> shift) & champsim::bitmask(champsim::lg2(DRAM_BANKS));
}

uint32_t MEMORY_CONTROLLER::dram_get_column(uint64_t address)
{
  int shift = champsim::lg2(DRAM_BANKS) + champsim::lg2(DRAM_CHANNELS) + LOG2_BLOCK_SIZE;
  return (address >> shift) & champsim::bitmask(champsim::lg2(DRAM_COLUMNS));
}

uint32_t MEMORY_CONTROLLER::dram_get_rank(uint64_t address)
{
  int shift = champsim::lg2(DRAM_BANKS) + champsim::lg2(DRAM_COLUMNS) + champsim::lg2(DRAM_CHANNELS) + LOG2_BLOCK_SIZE;
  return (address >> shift) & champsim::bitmask(champsim::lg2(DRAM_RANKS));
}

uint32_t MEMORY_CONTROLLER::dram_get_row(uint64_t address)
{
  int shift = champsim::lg2(DRAM_RANKS) + champsim::lg2(DRAM_BANKS) + champsim::lg2(DRAM_COLUMNS) + champsim::lg2(DRAM_CHANNELS) + LOG2_BLOCK_SIZE;
  return (address >> shift) & champsim::bitmask(champsim::lg2(DRAM_ROWS));
}

std::size_t MEMORY_CONTROLLER::get_occupancy(uint8_t queue_type, uint64_t address)
{
  uint32_t channel = dram_get_channel(address);
  if (queue_type == 1)
    return static_cast<std::size_t>(std::count_if(std::begin(channels[channel].RQ), std::end(channels[channel].RQ), is_valid<PACKET>()));
  else if (queue_type == 2)
    return static_cast<std::size_t>(std::count_if(std::begin(channels[channel].WQ), std::end(channels[channel].WQ), is_valid<PACKET>()));
  else if (queue_type == 3)
    return get_occupancy(1, address);

  return 0;
}

std::size_t MEMORY_CONTROLLER::get_size(uint8_t queue_type, uint64_t address)
{
  uint32_t channel = dram_get_channel(address);
  if (queue_type == 1)
    return channels[channel].RQ.size();
  else if (queue_type == 2)
    return channels[channel].WQ.size();
  else if (queue_type == 3)
    return get_size(1, address);

  return 0;
}

std::size_t MEMORY_CONTROLLER::size() const { return DRAM_CHANNELS * DRAM_RANKS * DRAM_BANKS * DRAM_ROWS * DRAM_COLUMNS * BLOCK_SIZE; }
