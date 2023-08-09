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
#include "deadlock.h"
#include "instruction.h"
#include "util/span.h"
#include <fmt/core.h>

uint64_t cycles(double time, int io_freq)
{
  std::fesetround(FE_UPWARD);
  auto result = std::lrint(time * io_freq);
  return result < 0 ? 0 : static_cast<uint64_t>(result);
}

MEMORY_CONTROLLER::MEMORY_CONTROLLER(double freq_scale, int io_freq, double t_rp, double t_rcd, double t_cas, double turnaround,
                                     std::vector<channel_type*>&& ul)
    : champsim::operable(freq_scale), queues(std::move(ul)), tRP(cycles(t_rp / 1000, io_freq)), tRCD(cycles(t_rcd / 1000, io_freq)),
      tCAS(cycles(t_cas / 1000, io_freq)), DRAM_DBUS_TURN_AROUND_TIME(cycles(turnaround / 1000, io_freq)),
      DRAM_DBUS_RETURN_TIME(cycles(std::ceil(BLOCK_SIZE) / std::ceil(DRAM_CHANNEL_WIDTH), 1))
{
}

long MEMORY_CONTROLLER::operate()
{
  long progress{0};

  initiate_requests();

  for (auto& channel : channels) {
    if (warmup) {
      for (auto& entry : channel.RQ) {
        if (entry.has_value()) {
          response_type response{entry->address, entry->v_address, entry->data, entry->pf_metadata, entry->instr_depend_on_me};
          for (auto ret : entry.value().to_return)
            ret->push_back(response);

          ++progress;
          entry.reset();
        }
      }

      for (auto& entry : channel.WQ) {
        if (entry.has_value()) {
          ++progress;
        }
        entry.reset();
      }
    }

    // Check for forwarding
    channel.check_collision();

    // Finish request
    if (channel.active_request != std::end(channel.bank_request) && channel.active_request->event_cycle <= current_cycle) {
      response_type response{channel.active_request->pkt->value().address, channel.active_request->pkt->value().v_address,
                             channel.active_request->pkt->value().data, channel.active_request->pkt->value().pf_metadata,
                             channel.active_request->pkt->value().instr_depend_on_me};
      for (auto ret : channel.active_request->pkt->value().to_return)
        ret->push_back(response);

      channel.active_request->valid = false;

      channel.active_request->pkt->reset();
      channel.active_request = std::end(channel.bank_request);
      ++progress;
    }

    // Check queue occupancy
    auto wq_occu = static_cast<std::size_t>(std::count_if(std::begin(channel.WQ), std::end(channel.WQ), [](const auto& x) { return x.has_value(); }));
    auto rq_occu = static_cast<std::size_t>(std::count_if(std::begin(channel.RQ), std::end(channel.RQ), [](const auto& x) { return x.has_value(); }));

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
          it->pkt->value().scheduled = false;
          it->pkt->value().event_cycle = current_cycle;
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
            ++channel.sim_stats.WQ_ROW_BUFFER_HIT;
          else
            ++channel.sim_stats.RQ_ROW_BUFFER_HIT;
        else if (channel.write_mode)
          ++channel.sim_stats.WQ_ROW_BUFFER_MISS;
        else
          ++channel.sim_stats.RQ_ROW_BUFFER_MISS;

        ++progress;
      } else {
        // Bus is congested
        if (channel.active_request != std::end(channel.bank_request))
          channel.sim_stats.dbus_cycle_congested += (channel.active_request->event_cycle - current_cycle);
        else
          channel.sim_stats.dbus_cycle_congested += (channel.dbus_cycle_available - current_cycle);
        ++channel.sim_stats.dbus_count_congested;
      }
    }

    // Look for queued packets that have not been scheduled
    auto next_schedule = [](const auto& lhs, const auto& rhs) {
      return !(rhs.has_value() && !rhs.value().scheduled) || ((lhs.has_value() && !lhs.value().scheduled) && lhs.value().event_cycle < rhs.value().event_cycle);
    };
    DRAM_CHANNEL::queue_type::iterator iter_next_schedule;
    if (channel.write_mode)
      iter_next_schedule = std::min_element(std::begin(channel.WQ), std::end(channel.WQ), next_schedule);
    else
      iter_next_schedule = std::min_element(std::begin(channel.RQ), std::end(channel.RQ), next_schedule);

    if (iter_next_schedule->has_value() && iter_next_schedule->value().event_cycle <= current_cycle) {
      auto op_rank = dram_get_rank(iter_next_schedule->value().address);
      auto op_bank = dram_get_bank(iter_next_schedule->value().address);
      auto op_row = dram_get_row(iter_next_schedule->value().address);

      auto op_idx = op_rank * DRAM_BANKS + op_bank;

      if (!channel.bank_request[op_idx].valid) {
        bool row_buffer_hit = (channel.bank_request[op_idx].open_row == op_row);

        // this bank is now busy
        channel.bank_request[op_idx] = {true, row_buffer_hit, op_row, current_cycle + tCAS + (row_buffer_hit ? 0 : tRP + tRCD), iter_next_schedule};

        iter_next_schedule->value().scheduled = true;
        iter_next_schedule->value().event_cycle = std::numeric_limits<uint64_t>::max();

        ++progress;
      }
    }
  }

  return progress;
}

void MEMORY_CONTROLLER::initialize()
{
  long long int dram_size = DRAM_CHANNELS * DRAM_RANKS * DRAM_BANKS * DRAM_ROWS * DRAM_COLUMNS * BLOCK_SIZE / 1024 / 1024; // in MiB
  fmt::print("Off-chip DRAM Size: ");
  if (dram_size > 1024)
    fmt::print("{} GiB", dram_size / 1024);
  else
    fmt::print("{} MiB", dram_size);
  fmt::print(" Channels: {} Width: {}-bit Data Race: {} MT/s\n", DRAM_CHANNELS, 8 * DRAM_CHANNEL_WIDTH, DRAM_IO_FREQ);
}

void MEMORY_CONTROLLER::begin_phase()
{
  std::size_t chan_idx = 0;
  for (auto& chan : channels) {
    DRAM_CHANNEL::stats_type new_stats;
    new_stats.name = "Channel " + std::to_string(chan_idx++);
    chan.sim_stats = new_stats;
  }

  for (auto ul : queues) {
    channel_type::stats_type ul_new_roi_stats, ul_new_sim_stats;
    ul->roi_stats = ul_new_roi_stats;
    ul->sim_stats = ul_new_sim_stats;
  }
}

void MEMORY_CONTROLLER::end_phase(unsigned)
{
  for (auto& chan : channels) {
    chan.roi_stats = chan.sim_stats;
  }
}

void DRAM_CHANNEL::check_collision()
{
  for (auto wq_it = std::begin(WQ); wq_it != std::end(WQ); ++wq_it) {
    if (wq_it->has_value() && !wq_it->value().forward_checked) {
      auto checker = [addr = wq_it->value().address, offset = LOG2_BLOCK_SIZE](const auto& pkt) {
        return pkt.has_value() && (pkt->address >> offset) == (addr >> offset);
      };
      if (auto found = std::find_if(std::begin(WQ), wq_it, checker); found != wq_it) { // Forward check
        wq_it->reset();
      } else if (found = std::find_if(std::next(wq_it), std::end(WQ), checker); found != std::end(WQ)) { // Backward check
        wq_it->reset();
      } else {
        wq_it->value().forward_checked = true;
      }
    }
  }

  for (auto rq_it = std::begin(RQ); rq_it != std::end(RQ); ++rq_it) {
    if (rq_it->has_value() && !rq_it->value().forward_checked) {
      auto checker = [addr = rq_it->value().address, offset = LOG2_BLOCK_SIZE](const auto& pkt) {
        return pkt.has_value() && (pkt->address >> offset) == (addr >> offset);
      };
      if (auto wq_it = std::find_if(std::begin(WQ), std::end(WQ), checker); wq_it != std::end(WQ)) {
        response_type response{rq_it->value().address, rq_it->value().v_address, rq_it->value().data, rq_it->value().pf_metadata,
                               rq_it->value().instr_depend_on_me};
        response.data = wq_it->value().data;
        for (auto ret : rq_it->value().to_return)
          ret->push_back(response);

        rq_it->reset();
      } else if (auto found = std::find_if(std::begin(RQ), rq_it, checker); found != rq_it) {
        auto instr_copy = std::move(found->value().instr_depend_on_me);
        auto ret_copy = std::move(found->value().to_return);

        std::set_union(std::begin(instr_copy), std::end(instr_copy), std::begin(rq_it->value().instr_depend_on_me), std::end(rq_it->value().instr_depend_on_me),
                       std::back_inserter(found->value().instr_depend_on_me), ooo_model_instr::program_order);
        std::set_union(std::begin(ret_copy), std::end(ret_copy), std::begin(rq_it->value().to_return), std::end(rq_it->value().to_return),
                       std::back_inserter(found->value().to_return));

        rq_it->reset();
      } else if (found = std::find_if(std::next(rq_it), std::end(RQ), checker); found != std::end(RQ)) {
        auto instr_copy = std::move(found->value().instr_depend_on_me);
        auto ret_copy = std::move(found->value().to_return);

        std::set_union(std::begin(instr_copy), std::end(instr_copy), std::begin(rq_it->value().instr_depend_on_me), std::end(rq_it->value().instr_depend_on_me),
                       std::back_inserter(found->value().instr_depend_on_me), ooo_model_instr::program_order);
        std::set_union(std::begin(ret_copy), std::end(ret_copy), std::begin(rq_it->value().to_return), std::end(rq_it->value().to_return),
                       std::back_inserter(found->value().to_return));

        rq_it->reset();
      } else {
        rq_it->value().forward_checked = true;
      }
    }
  }
}

void MEMORY_CONTROLLER::initiate_requests()
{
  // Initiate read requests
  for (auto ul : queues) {
    for (auto q : {std::ref(ul->RQ), std::ref(ul->PQ)}) {
      auto [begin, end] = champsim::get_span_p(std::cbegin(q.get()), std::cend(q.get()), [ul, this](const auto& pkt) { return this->add_rq(pkt, ul); });
      q.get().erase(begin, end);
    }

    // Initiate write requests
    auto [wq_begin, wq_end] = champsim::get_span_p(std::cbegin(ul->WQ), std::cend(ul->WQ), [this](const auto& pkt) { return this->add_wq(pkt); });
    ul->WQ.erase(wq_begin, wq_end);
  }
}

DRAM_CHANNEL::request_type::request_type(typename champsim::channel::request_type req)
    : pf_metadata(req.pf_metadata), address(req.address), v_address(req.address), data(req.data), instr_depend_on_me(req.instr_depend_on_me)
{
  asid[0] = req.asid[0];
  asid[1] = req.asid[1];
}

bool MEMORY_CONTROLLER::add_rq(const request_type& packet, champsim::channel* ul)
{
  auto& channel = channels[dram_get_channel(packet.address)];

  // Find empty slot
  if (auto rq_it = std::find_if_not(std::begin(channel.RQ), std::end(channel.RQ), [](const auto& pkt) { return pkt.has_value(); });
      rq_it != std::end(channel.RQ)) {
    *rq_it = DRAM_CHANNEL::request_type{packet};
    rq_it->value().forward_checked = false;
    rq_it->value().event_cycle = current_cycle;
    if (packet.response_requested)
      rq_it->value().to_return = {&ul->returned};

    return true;
  }

  return false;
}

bool MEMORY_CONTROLLER::add_wq(const request_type& packet)
{
  auto& channel = channels[dram_get_channel(packet.address)];

  // search for the empty index
  if (auto wq_it = std::find_if_not(std::begin(channel.WQ), std::end(channel.WQ), [](const auto& pkt) { return pkt.has_value(); });
      wq_it != std::end(channel.WQ)) {
    *wq_it = DRAM_CHANNEL::request_type{packet};
    wq_it->value().forward_checked = false;
    wq_it->value().event_cycle = current_cycle;

    return true;
  }

  ++channel.sim_stats.WQ_FULL;
  return false;
}

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

std::size_t MEMORY_CONTROLLER::size() const { return DRAM_CHANNELS * DRAM_RANKS * DRAM_BANKS * DRAM_ROWS * DRAM_COLUMNS * BLOCK_SIZE; }

// LCOV_EXCL_START Exclude the following function from LCOV
void MEMORY_CONTROLLER::print_deadlock()
{
  int j = 0;
  for (auto& chan : channels) {
    fmt::print("DRAM Channel {}\n", j++);
    chan.print_deadlock();
  }
}

void DRAM_CHANNEL::print_deadlock()
{
  std::string_view q_writer{"instr_id: {} address: {:#x} v_addr: {:#x} type: {} translated: {}"};
  auto q_entry_pack = [](const auto& entry) {
    return std::tuple{entry->address, entry->v_address};
  };

  champsim::range_print_deadlock(RQ, "RQ", q_writer, q_entry_pack);
  champsim::range_print_deadlock(WQ, "WQ", q_writer, q_entry_pack);
}
// LCOV_EXCL_STOP
