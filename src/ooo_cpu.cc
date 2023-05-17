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

#include "ooo_cpu.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "cache.h"
#include "champsim.h"
#include "instruction.h"

std::tuple<uint64_t, uint64_t, uint64_t> elapsed_time();

void O3_CPU::operate()
{
  ROB.operate();
  handle_memory_return();
  dispatch_instruction(); // dispatch
  decode_instruction();   // decode
  promote_to_decode();

  fetch_instruction(); // fetch
  check_dib();
  initialize_instruction();

  // heartbeat
  if (show_heartbeat && (ROB.retired_count() >= next_print_instruction)) {
    auto [elapsed_hour, elapsed_minute, elapsed_second] = elapsed_time();

    auto heartbeat_instr{std::ceil(ROB.retired_count() - last_heartbeat_instr)};
    auto heartbeat_cycle{std::ceil(current_cycle - last_heartbeat_cycle)};

    auto phase_instr{std::ceil(ROB.retired_count() - begin_phase_instr)};
    auto phase_cycle{std::ceil(current_cycle - begin_phase_cycle)};

    std::cout << "Heartbeat CPU " << cpu << " instructions: " << ROB.retired_count() << " cycles: " << current_cycle;
    std::cout << " heartbeat IPC: " << heartbeat_instr / heartbeat_cycle;
    std::cout << " cumulative IPC: " << phase_instr / phase_cycle;
    std::cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << std::endl;
    next_print_instruction += STAT_PRINTING_PERIOD;

    last_heartbeat_instr = ROB.retired_count();
    last_heartbeat_cycle = current_cycle;
  }

  // Check for deadlock
  if (ROB.is_deadlocked())
    throw champsim::deadlock{cpu};
}

void O3_CPU::initialize()
{
  // BRANCH PREDICTOR & BTB
  impl_initialize_branch_predictor();
  impl_initialize_btb();
}

void O3_CPU::begin_phase()
{
  ROB.warmup = warmup;
  begin_phase_instr = ROB.retired_count();
  begin_phase_cycle = current_cycle;

  // Record where the next phase begins
  stats_type stats;
  stats.name = "CPU " + std::to_string(cpu);
  stats.begin_instrs = ROB.retired_count();
  stats.begin_cycles = current_cycle;
  sim_stats = stats;
}

void O3_CPU::end_phase(unsigned finished_cpu)
{
  // Record where the phase ended (overwrite if this is later)
  sim_stats.end_instrs = ROB.retired_count();
  sim_stats.end_cycles = current_cycle;

  if (finished_cpu == this->cpu) {
    finish_phase_instr = ROB.retired_count();
    finish_phase_cycle = current_cycle;

    roi_stats = sim_stats;
  }
}

void O3_CPU::initialize_instruction()
{
  auto instrs_to_read_this_cycle = std::min(FETCH_WIDTH, static_cast<long>(IFETCH_BUFFER_SIZE - std::size(IFETCH_BUFFER)));

  while (current_cycle >= fetch_resume_cycle && current_cycle >= ROB.get_stall_resume_cycle() && instrs_to_read_this_cycle > 0 && !std::empty(input_queue)) {
    instrs_to_read_this_cycle--;

    auto stop_fetch = do_init_instruction(input_queue.front());
    if (stop_fetch)
      instrs_to_read_this_cycle = 0;

    // Add to IFETCH_BUFFER
    IFETCH_BUFFER.push_back(input_queue.front());
    input_queue.pop_front();

    IFETCH_BUFFER.back().event_cycle = current_cycle;
  }
}

namespace
{
void do_stack_pointer_folding(ooo_model_instr& arch_instr)
{
  // The exact, true value of the stack pointer for any given instruction can usually be determined immediately after the instruction is decoded without
  // waiting for the stack pointer's dependency chain to be resolved.
  bool writes_sp = std::count(std::begin(arch_instr.destination_registers), std::end(arch_instr.destination_registers), champsim::REG_STACK_POINTER);
  if (writes_sp) {
    // Avoid creating register dependencies on the stack pointer for calls, returns, pushes, and pops, but not for variable-sized changes in the
    // stack pointer position. reads_other indicates that the stack pointer is being changed by a variable amount, which can't be determined before
    // execution.
    bool reads_other = std::count_if(std::begin(arch_instr.source_registers), std::end(arch_instr.source_registers), [](uint8_t r) {
      return r != champsim::REG_STACK_POINTER && r != champsim::REG_FLAGS && r != champsim::REG_INSTRUCTION_POINTER;
    });
    if ((arch_instr.is_branch != 0) || !(std::empty(arch_instr.destination_memory) && std::empty(arch_instr.source_memory)) || (!reads_other)) {
      auto nonsp_end = std::remove(std::begin(arch_instr.destination_registers), std::end(arch_instr.destination_registers), champsim::REG_STACK_POINTER);
      arch_instr.destination_registers.erase(nonsp_end, std::end(arch_instr.destination_registers));
    }
  }
}
} // namespace

bool O3_CPU::do_predict_branch(ooo_model_instr& arch_instr)
{
  bool stop_fetch = false;

  // handle branch prediction for all instructions as at this point we do not know if the instruction is a branch
  sim_stats.total_branch_types[arch_instr.branch_type]++;
  auto [predicted_branch_target, always_taken] = impl_btb_prediction(arch_instr.ip);
  arch_instr.branch_prediction = impl_predict_branch(arch_instr.ip) || always_taken;
  if (arch_instr.branch_prediction == 0)
    predicted_branch_target = 0;

  if (arch_instr.is_branch) {
    if constexpr (champsim::debug_print) {
      std::cout << "[BRANCH] instr_id: " << arch_instr.instr_id << " ip: " << std::hex << arch_instr.ip << std::dec << " taken: " << +arch_instr.branch_taken
                << std::endl;
    }

    if (predicted_branch_target != arch_instr.branch_target
        || (arch_instr.branch_type == BRANCH_CONDITIONAL
            && arch_instr.branch_taken != arch_instr.branch_prediction)) { // conditional branches are re-evaluated at decode when the target is computed
      sim_stats.total_rob_occupancy_at_branch_mispredict += ROB.occupancy();
      sim_stats.branch_type_misses[arch_instr.branch_type]++;
      if (!warmup) {
        if (arch_instr.has_direct_branch_target())
          fetch_resume_cycle = std::numeric_limits<uint64_t>::max();
        if (arch_instr.has_indirect_branch_target())
          ROB.set_stall();
        stop_fetch = true;
        arch_instr.branch_mispredicted = 1;
      }
    } else {
      stop_fetch = arch_instr.branch_taken; // if correctly predicted taken, then we can't fetch anymore instructions this cycle
    }

    l1i->impl_prefetcher_branch_operate(arch_instr.ip, arch_instr.branch_type, predicted_branch_target);
    impl_update_btb(arch_instr.ip, arch_instr.branch_target, arch_instr.branch_taken, arch_instr.branch_type);
    impl_last_branch_result(arch_instr.ip, arch_instr.branch_target, arch_instr.branch_taken, arch_instr.branch_type);
  }

  return stop_fetch;
}

bool O3_CPU::do_init_instruction(ooo_model_instr& arch_instr)
{
  // fast warmup eliminates register dependencies between instructions branch predictor, cache contents, and prefetchers are still warmed up
  if (warmup) {
    arch_instr.source_registers.clear();
    arch_instr.destination_registers.clear();
  }

  ::do_stack_pointer_folding(arch_instr);
  return do_predict_branch(arch_instr);
}

void O3_CPU::check_dib()
{
  // scan through IFETCH_BUFFER to find instructions that hit in the decoded instruction buffer
  auto begin = std::find_if(std::begin(IFETCH_BUFFER), std::end(IFETCH_BUFFER), [](const ooo_model_instr& x) { return !x.dib_checked; });
  auto [window_begin, window_end] = champsim::get_span(begin, std::end(IFETCH_BUFFER), FETCH_WIDTH);
  for (auto it = window_begin; it != window_end; ++it)
    do_check_dib(*it);
}

void O3_CPU::do_check_dib(ooo_model_instr& instr)
{
  // Check DIB to see if we recently fetched this line
  if (auto dib_result = DIB.check_hit(instr.ip); dib_result) {
    // The cache line is in the L0, so we can mark this as complete
    instr.fetched = COMPLETED;

    // Also mark it as decoded
    instr.decoded = COMPLETED;

    // It can be acted on immediately
    instr.event_cycle = current_cycle;
  }

  instr.dib_checked = COMPLETED;
}

void O3_CPU::fetch_instruction()
{
  // Fetch a single cache line
  auto fetch_ready = [](const ooo_model_instr& x) {
    return x.dib_checked == COMPLETED && !x.fetched;
  };

  // Find the chunk of instructions in the block
  auto no_match_ip = [](const auto& lhs, const auto& rhs) {
    return (lhs.ip >> LOG2_BLOCK_SIZE) != (rhs.ip >> LOG2_BLOCK_SIZE);
  };

  auto l1i_req_begin = std::find_if(std::begin(IFETCH_BUFFER), std::end(IFETCH_BUFFER), fetch_ready);
  for (auto to_read = L1I_BANDWIDTH; to_read > 0 && l1i_req_begin != std::end(IFETCH_BUFFER); --to_read) {
    auto l1i_req_end = std::adjacent_find(l1i_req_begin, std::end(IFETCH_BUFFER), no_match_ip);
    if (l1i_req_end != std::end(IFETCH_BUFFER))
      l1i_req_end = std::next(l1i_req_end); // adjacent_find returns the first of the non-equal elements

    // Issue to L1I
    auto success = do_fetch_instruction(l1i_req_begin, l1i_req_end);
    if (success)
      std::for_each(l1i_req_begin, l1i_req_end, [](auto& x) { x.fetched = INFLIGHT; });

    l1i_req_begin = std::find_if(l1i_req_end, std::end(IFETCH_BUFFER), fetch_ready);
  }
}

bool O3_CPU::do_fetch_instruction(std::deque<ooo_model_instr>::iterator begin, std::deque<ooo_model_instr>::iterator end)
{
  champsim::CacheBus::request_type fetch_packet;
  fetch_packet.v_address = begin->ip;
  fetch_packet.instr_id = begin->instr_id;
  fetch_packet.ip = begin->ip;
  std::transform(begin, end, std::back_inserter(fetch_packet.instr_depend_on_me), [](const auto& x){ return x.instr_id; });

  if constexpr (champsim::debug_print) {
    std::cout << "[IFETCH] " << __func__ << " instr_id: " << begin->instr_id << std::hex;
    std::cout << " ip: " << begin->ip << std::dec << " dependents: " << std::size(fetch_packet.instr_depend_on_me);
    std::cout << " event_cycle: " << begin->event_cycle << std::endl;
  }

  return L1I_bus.issue_read(fetch_packet);
}

void O3_CPU::promote_to_decode()
{
  auto available_fetch_bandwidth = std::min<long>(FETCH_WIDTH, DECODE_BUFFER_SIZE - std::size(DECODE_BUFFER));
  auto [window_begin, window_end] = champsim::get_span_p(std::begin(IFETCH_BUFFER), std::end(IFETCH_BUFFER), available_fetch_bandwidth,
                                                         [cycle = current_cycle](const auto& x) { return x.fetched == COMPLETED && x.event_cycle <= cycle; });
  std::for_each(window_begin, window_end,
                [cycle = current_cycle, lat = DECODE_LATENCY, warmup = warmup](auto& x) { return x.event_cycle = cycle + ((warmup || x.decoded) ? 0 : lat); });
  std::move(window_begin, window_end, std::back_inserter(DECODE_BUFFER));
  IFETCH_BUFFER.erase(window_begin, window_end);

  // LCOV_EXCL_START check for deadlock
  if (!std::empty(IFETCH_BUFFER) && (IFETCH_BUFFER.front().event_cycle + champsim::DEADLOCK_CYCLE) <= current_cycle)
    throw champsim::deadlock{cpu};
  // LCOV_EXCL_STOP
}

void O3_CPU::decode_instruction()
{
  auto available_decode_bandwidth = std::min<long>(DECODE_WIDTH, DISPATCH_BUFFER_SIZE - std::size(DISPATCH_BUFFER));
  auto [window_begin, window_end] = champsim::get_span_p(std::begin(DECODE_BUFFER), std::end(DECODE_BUFFER), available_decode_bandwidth,
                                                         [cycle = current_cycle](const auto& x) { return x.event_cycle <= cycle; });

  // Send decoded instructions to dispatch
  std::for_each(window_begin, window_end, [&, this](auto& db_entry) {
    this->do_dib_update(db_entry);

    // These branches resume fetch at decode
    if (db_entry.branch_mispredicted && db_entry.has_direct_branch_target()) {
      db_entry.branch_mispredicted = 0; // clear the branch_mispredicted bit so we don't attempt to resume fetch again at execute
      this->fetch_resume_cycle = this->current_cycle + BRANCH_MISPREDICT_PENALTY; // pay misprediction penalty
    }

    // Add to dispatch
    db_entry.event_cycle = this->current_cycle + (this->warmup ? 0 : this->DISPATCH_LATENCY);
  });

  std::move(window_begin, window_end, std::back_inserter(DISPATCH_BUFFER));
  DECODE_BUFFER.erase(window_begin, window_end);

  // LCOV_EXCL_START check for deadlock
  if (!std::empty(DECODE_BUFFER) && (DECODE_BUFFER.front().event_cycle + champsim::DEADLOCK_CYCLE) <= current_cycle)
    throw champsim::deadlock{cpu};
  // LCOV_EXCL_STOP
}

void O3_CPU::do_dib_update(const ooo_model_instr& instr) { DIB.fill(instr.ip); }

void O3_CPU::dispatch_instruction()
{
  std::size_t available_dispatch_bandwidth = DISPATCH_WIDTH;

  // dispatch DISPATCH_WIDTH instructions into the ROB
  while (available_dispatch_bandwidth > 0 && !std::empty(DISPATCH_BUFFER) && DISPATCH_BUFFER.front().event_cycle < current_cycle && ROB.would_accept(DISPATCH_BUFFER.front())) {
    ROB.push_back(std::move(DISPATCH_BUFFER.front()));
    DISPATCH_BUFFER.pop_front();

    available_dispatch_bandwidth--;
  }

  // LCOV_EXCL_START check for deadlock
  if (!std::empty(DISPATCH_BUFFER) && (DISPATCH_BUFFER.front().event_cycle + champsim::DEADLOCK_CYCLE) <= current_cycle)
    throw champsim::deadlock{cpu};
  // LCOV_EXCL_STOP
}

void O3_CPU::handle_memory_return()
{
  for (auto l1i_bw = FETCH_WIDTH, to_read = L1I_BANDWIDTH; l1i_bw > 0 && to_read > 0 && !L1I_bus.lower_level->returned.empty(); --to_read) {
    auto& l1i_entry = L1I_bus.lower_level->returned.front();

    while (l1i_bw > 0 && !l1i_entry.instr_depend_on_me.empty()) {
      auto fetched_it = std::find_if(std::begin(IFETCH_BUFFER), std::end(IFETCH_BUFFER), [id=l1i_entry.instr_depend_on_me.front()](const auto& x) { return x.instr_id == id; });
      if ((fetched_it->ip >> LOG2_BLOCK_SIZE) == (l1i_entry.v_address >> LOG2_BLOCK_SIZE) && fetched_it->fetched != 0) {
        fetched_it->fetched = COMPLETED;
        --l1i_bw;

        if constexpr (champsim::debug_print) {
          std::cout << "[IFETCH] " << __func__ << " instr_id: " << fetched_it->instr_id << " fetch completed" << std::endl;
        }
      }

      l1i_entry.instr_depend_on_me.erase(std::begin(l1i_entry.instr_depend_on_me));
    }

    // remove this entry if we have serviced all of its instructions
    if (l1i_entry.instr_depend_on_me.empty())
      L1I_bus.lower_level->returned.pop_front();
  }
}

// LCOV_EXCL_START Exclude the following function from LCOV
void O3_CPU::print_deadlock()
{
  std::cout << "DEADLOCK! CPU " << cpu << " cycle " << current_cycle << std::endl;

  if (!std::empty(IFETCH_BUFFER)) {
    std::cout << "IFETCH_BUFFER head";
    std::cout << " instr_id: " << IFETCH_BUFFER.front().instr_id;
    std::cout << " fetched: " << +IFETCH_BUFFER.front().fetched;
    std::cout << " scheduled: " << +IFETCH_BUFFER.front().scheduled;
    std::cout << " executed: " << +IFETCH_BUFFER.front().executed;
    std::cout << " num_reg_dependent: " << +IFETCH_BUFFER.front().num_reg_dependent;
    std::cout << " num_mem_ops: " << IFETCH_BUFFER.front().num_mem_ops() - IFETCH_BUFFER.front().completed_mem_ops;
    std::cout << " event: " << IFETCH_BUFFER.front().event_cycle;
    std::cout << std::endl;
  } else {
    std::cout << "IFETCH_BUFFER empty" << std::endl;
  }

  ROB.print_deadlock();
}
// LCOV_EXCL_STOP

