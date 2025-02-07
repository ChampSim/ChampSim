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
#include <chrono>
#include <cmath>
#include <numeric>
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/ranges.h>

#include "cache.h"
#include "champsim.h"
#include "deadlock.h"
#include "instruction.h"
#include "util/span.h"

std::chrono::seconds elapsed_time();

constexpr long long STAT_PRINTING_PERIOD = 10000000;

long O3_CPU::operate()
{
  long progress{0};
  progress += retire_rob();                    // retire
  progress += complete_inflight_instruction(); // finalize execution
  progress += execute_instruction();           // execute instructions
  progress += schedule_instruction();          // schedule instructions
  progress += handle_memory_return();          // finalize memory transactions
  progress += operate_lsq();                   // execute memory transactions

  progress += dispatch_instruction(); // dispatch
  progress += decode_instruction();   // decode
  progress += promote_to_decode();

  progress += fetch_instruction(); // fetch
  progress += check_dib();
  initialize_instruction();

  // heartbeat
  if (show_heartbeat && (num_retired >= (last_heartbeat_instr + STAT_PRINTING_PERIOD))) {
    using double_duration = std::chrono::duration<double, typename champsim::chrono::picoseconds::period>;
    auto heartbeat_instr{std::ceil(num_retired - last_heartbeat_instr)};
    auto heartbeat_cycle{double_duration{current_time - last_heartbeat_time} / clock_period};

    auto phase_instr{std::ceil(num_retired - begin_phase_instr)};
    auto phase_cycle{double_duration{current_time - begin_phase_time} / clock_period};

    fmt::print("Heartbeat CPU {} instructions: {} cycles: {} heartbeat IPC: {:.4g} cumulative IPC: {:.4g} (Simulation time: {:%H hr %M min %S sec})\n", cpu,
               num_retired, current_time.time_since_epoch() / clock_period, heartbeat_instr / heartbeat_cycle, phase_instr / phase_cycle, elapsed_time());

    last_heartbeat_instr = num_retired;
    last_heartbeat_time = current_time;
  }

  return progress;
}

void O3_CPU::initialize()
{
  // BRANCH PREDICTOR & BTB
  impl_initialize_branch_predictor();
  impl_initialize_btb();
}

void O3_CPU::begin_phase()
{
  begin_phase_instr = num_retired;
  begin_phase_time = current_time;

  // Record where the next phase begins
  stats_type stats;
  stats.name = "CPU " + std::to_string(cpu);
  stats.begin_instrs = num_retired;
  stats.begin_cycles = begin_phase_time.time_since_epoch() / clock_period;
  sim_stats = stats;
}

void O3_CPU::end_phase(unsigned finished_cpu)
{
  // Record where the phase ended (overwrite if this is later)
  sim_stats.end_instrs = num_retired;
  sim_stats.end_cycles = current_time.time_since_epoch() / clock_period;

  if (finished_cpu == this->cpu) {
    finish_phase_instr = num_retired;
    finish_phase_time = current_time;

    roi_stats = sim_stats;
  }
}

void O3_CPU::initialize_instruction()
{
  champsim::bandwidth instrs_to_read_this_cycle{
      std::min(FETCH_WIDTH, champsim::bandwidth::maximum_type{static_cast<long>(IFETCH_BUFFER_SIZE - std::size(IFETCH_BUFFER))})};

  bool stop_fetch = false;
  while (current_time >= fetch_resume_time && instrs_to_read_this_cycle.has_remaining() && !stop_fetch && !std::empty(input_queue)) {
    instrs_to_read_this_cycle.consume();

    stop_fetch = do_init_instruction(input_queue.front());

    // Add to IFETCH_BUFFER
    IFETCH_BUFFER.push_back(input_queue.front());
    input_queue.pop_front();

    IFETCH_BUFFER.back().ready_time = current_time;
  }
}

namespace
{
void do_stack_pointer_folding(ooo_model_instr& arch_instr)
{
  // The exact, true value of the stack pointer for any given instruction can usually be determined immediately after the instruction is decoded without
  // waiting for the stack pointer's dependency chain to be resolved.
  bool writes_sp = (std::count(std::begin(arch_instr.destination_registers), std::end(arch_instr.destination_registers), champsim::REG_STACK_POINTER) > 0);
  if (writes_sp) {
    // Avoid creating register dependencies on the stack pointer for calls, returns, pushes, and pops, but not for variable-sized changes in the
    // stack pointer position. reads_other indicates that the stack pointer is being changed by a variable amount, which can't be determined before
    // execution.
    bool reads_other =
        (std::count_if(std::begin(arch_instr.source_registers), std::end(arch_instr.source_registers),
                       [](auto r) { return r != champsim::REG_STACK_POINTER && r != champsim::REG_FLAGS && r != champsim::REG_INSTRUCTION_POINTER; })
         > 0);
    if ((arch_instr.is_branch) || !(std::empty(arch_instr.destination_memory) && std::empty(arch_instr.source_memory)) || (!reads_other)) {
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
  sim_stats.total_branch_types.increment(arch_instr.branch);
  auto [predicted_branch_target, always_taken] = impl_btb_prediction(arch_instr.ip, arch_instr.branch);
  arch_instr.branch_prediction = impl_predict_branch(arch_instr.ip, predicted_branch_target, always_taken, arch_instr.branch) || always_taken;
  if (!arch_instr.branch_prediction) {
    predicted_branch_target = champsim::address{};
  }

  if (arch_instr.is_branch) {
    if constexpr (champsim::debug_print) {
      fmt::print("[BRANCH] instr_id: {} ip: {} taken: {}\n", arch_instr.instr_id, arch_instr.ip, arch_instr.branch_taken);
    }

    // call code prefetcher every time the branch predictor is used
    l1i->impl_prefetcher_branch_operate(arch_instr.ip, arch_instr.branch, predicted_branch_target);

    if (predicted_branch_target != arch_instr.branch_target
        || (((arch_instr.branch == BRANCH_CONDITIONAL) || (arch_instr.branch == BRANCH_OTHER))
            && arch_instr.branch_taken != arch_instr.branch_prediction)) { // conditional branches are re-evaluated at decode when the target is computed
      sim_stats.total_rob_occupancy_at_branch_mispredict += std::size(ROB);
      sim_stats.branch_type_misses.increment(arch_instr.branch);
      if (!warmup) {
        fetch_resume_time = champsim::chrono::clock::time_point::max();
        stop_fetch = true;
        arch_instr.branch_mispredicted = true;
      }
    } else {
      stop_fetch = arch_instr.branch_taken; // if correctly predicted taken, then we can't fetch anymore instructions this cycle
    }

    impl_update_btb(arch_instr.ip, arch_instr.branch_target, arch_instr.branch_taken, arch_instr.branch);
    impl_last_branch_result(arch_instr.ip, arch_instr.branch_target, arch_instr.branch_taken, arch_instr.branch);
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

long O3_CPU::check_dib()
{
  // scan through IFETCH_BUFFER to find instructions that hit in the decoded instruction buffer
  auto begin = std::find_if(std::begin(IFETCH_BUFFER), std::end(IFETCH_BUFFER), [](const ooo_model_instr& x) { return !x.dib_checked; });
  auto [window_begin, window_end] = champsim::get_span(begin, std::end(IFETCH_BUFFER), champsim::bandwidth{FETCH_WIDTH});
  std::for_each(window_begin, window_end, [this](auto& ifetch_entry) { this->do_check_dib(ifetch_entry); });
  return std::distance(window_begin, window_end);
}

void O3_CPU::do_check_dib(ooo_model_instr& instr)
{
  // Check DIB to see if we recently fetched this line
  auto dib_result = DIB.check_hit(instr.ip);
  if (dib_result) {
    // The cache line is in the L0, so we can mark this as complete
    instr.fetch_completed = true;

    // Also mark it as decoded
    instr.decoded = true;

    // It can be acted on immediately
    instr.ready_time = current_time;
  }

  instr.dib_checked = true;

  if constexpr (champsim::debug_print) {
    fmt::print("[DIB] {} instr_id: {} ip: {:#x} hit: {} cycle: {}\n", __func__, instr.instr_id, instr.ip, dib_result.has_value(),
               current_time.time_since_epoch() / clock_period);
  }
}

long O3_CPU::fetch_instruction()
{
  long progress{0};

  // Fetch a single cache line
  auto fetch_ready = [](const ooo_model_instr& x) {
    return x.dib_checked && !x.fetch_issued;
  };

  // Find the chunk of instructions in the block
  auto no_match_ip = [](const auto& lhs, const auto& rhs) {
    return champsim::block_number{lhs.ip} != champsim::block_number{rhs.ip};
  };

  auto l1i_req_begin = std::find_if(std::begin(IFETCH_BUFFER), std::end(IFETCH_BUFFER), fetch_ready);
  for (champsim::bandwidth to_read{L1I_BANDWIDTH}; to_read.has_remaining() && l1i_req_begin != std::end(IFETCH_BUFFER); to_read.consume()) {
    auto l1i_req_end = std::adjacent_find(l1i_req_begin, std::end(IFETCH_BUFFER), no_match_ip);
    if (l1i_req_end != std::end(IFETCH_BUFFER)) {
      l1i_req_end = std::next(l1i_req_end); // adjacent_find returns the first of the non-equal elements
    }

    // Issue to L1I
    auto success = do_fetch_instruction(l1i_req_begin, l1i_req_end);
    if (success) {
      std::for_each(l1i_req_begin, l1i_req_end, [](auto& x) { x.fetch_issued = true; });
      ++progress;
    }

    l1i_req_begin = std::find_if(l1i_req_end, std::end(IFETCH_BUFFER), fetch_ready);
  }

  return progress;
}

bool O3_CPU::do_fetch_instruction(std::deque<ooo_model_instr>::iterator begin, std::deque<ooo_model_instr>::iterator end)
{
  CacheBus::request_type fetch_packet;
  fetch_packet.v_address = begin->ip;
  fetch_packet.instr_id = begin->instr_id;
  fetch_packet.ip = begin->ip;

  std::transform(begin, end, std::back_inserter(fetch_packet.instr_depend_on_me), [](const auto& instr) { return instr.instr_id; });

  if constexpr (champsim::debug_print) {
    fmt::print("[IFETCH] {} instr_id: {} ip: {} dependents: {} event_cycle: {}\n", __func__, begin->instr_id, begin->ip,
               std::size(fetch_packet.instr_depend_on_me), begin->ready_time.time_since_epoch() / clock_period);
  }

  return L1I_bus.issue_read(fetch_packet);
}

long O3_CPU::promote_to_decode()
{
  auto is_decoded = [](const ooo_model_instr& x) {
    return x.decoded;
  };

  auto fetch_complete_and_ready = [time = current_time](const auto& x) {
    return x.fetch_completed && x.ready_time <= time;
  };

  champsim::bandwidth available_fetch_bandwidth{
      std::min(FETCH_WIDTH, std::min(champsim::bandwidth::maximum_type{static_cast<long>(DIB_HIT_BUFFER_SIZE - std::size(DIB_HIT_BUFFER))},
                                     champsim::bandwidth::maximum_type{static_cast<long>(DECODE_BUFFER_SIZE - std::size(DECODE_BUFFER))}))};

  auto fetched_check_end = std::find_if(std::begin(IFETCH_BUFFER), std::end(IFETCH_BUFFER), [](const ooo_model_instr& x) { return !x.fetch_completed; });
  // find the first not fetch completed
  auto [window_begin, window_end] = champsim::get_span_p(std::begin(IFETCH_BUFFER), fetched_check_end, available_fetch_bandwidth, fetch_complete_and_ready);
  auto decoded_window_end = std::stable_partition(window_begin, window_end, is_decoded); // reorder instructions
  auto mark_for_decode = [time = current_time, lat = DECODE_LATENCY, warmup = warmup](auto& x) {
    return x.ready_time = time + (warmup ? champsim::chrono::clock::duration{} : lat);
  };
  // to DIB_HIT_BUFFER
  auto mark_for_dib = [time = current_time, lat = DIB_HIT_LATENCY, warmup = warmup](auto& x) {
    return x.ready_time = time + lat;
  };

  std::for_each(window_begin, decoded_window_end, mark_for_dib); // assume DECODE_LATENCY = DIB_HIT_LATENCY
  std::move(window_begin, decoded_window_end, std::back_inserter(DIB_HIT_BUFFER));
  // to DECODE_BUFFER

  std::for_each(decoded_window_end, window_end, mark_for_decode);
  std::move(decoded_window_end, window_end, std::back_inserter(DECODE_BUFFER));

  long progress{std::distance(window_begin, window_end)};
  IFETCH_BUFFER.erase(window_begin, window_end);
  return progress;
}
long O3_CPU::decode_instruction()
{
  auto is_ready = [time = current_time](const auto& x) {
    return x.ready_time <= time;
  };

  auto dib_hit_buffer_begin = std::begin(DIB_HIT_BUFFER);
  auto dib_hit_buffer_end = dib_hit_buffer_begin;
  auto decode_buffer_begin = std::begin(DECODE_BUFFER);
  auto decode_buffer_end = decode_buffer_begin;

  champsim::bandwidth available_decode_bandwidth{DECODE_WIDTH};

  // bw move instructions to dispatch_buffer
  champsim::bandwidth available_dib_inorder_bandwidth{
      std::min(DIB_INORDER_WIDTH, champsim::bandwidth::maximum_type{static_cast<long>(DISPATCH_BUFFER_SIZE - std::size(DISPATCH_BUFFER))})};

  // conditions choose how many instructions sent to dispatch_buffer
  while (dib_hit_buffer_end != std::end(DIB_HIT_BUFFER) && decode_buffer_end != std::end(DECODE_BUFFER) && available_dib_inorder_bandwidth.has_remaining()
         && available_decode_bandwidth.has_remaining() && is_ready(std::min(*dib_hit_buffer_end, *decode_buffer_end, ooo_model_instr::program_order))) {
    if (ooo_model_instr::program_order(*dib_hit_buffer_end, *decode_buffer_end)) {
      dib_hit_buffer_end++;
      available_dib_inorder_bandwidth.consume();
    } else {
      decode_buffer_end++;
      available_dib_inorder_bandwidth.consume();
      available_decode_bandwidth.consume();
    }
  }
  while (dib_hit_buffer_end != std::end(DIB_HIT_BUFFER) && available_dib_inorder_bandwidth.has_remaining() && is_ready(*dib_hit_buffer_end)
         && (decode_buffer_end == std::end(DECODE_BUFFER) || ooo_model_instr::program_order(*dib_hit_buffer_end, *decode_buffer_end))) {
    dib_hit_buffer_end++;
    available_dib_inorder_bandwidth.consume();
  }
  while (decode_buffer_end != std::end(DECODE_BUFFER) && available_dib_inorder_bandwidth.has_remaining() && available_decode_bandwidth.has_remaining()
         && is_ready(*decode_buffer_end)
         && (dib_hit_buffer_end == std::end(DIB_HIT_BUFFER) || ooo_model_instr::program_order(*decode_buffer_end, *dib_hit_buffer_end))) {
    decode_buffer_end++;
    available_dib_inorder_bandwidth.consume();
    available_decode_bandwidth.consume();
  }

  // decode instructions have not decoded, merge instructions with dib_hit_buffer then send to dispatch_buffer
  auto do_decode = [&, this](auto& db_entry) {
    this->do_dib_update(db_entry);

    // Resume fetch
    if (db_entry.branch_mispredicted) {
      // These branches detect the misprediction at decode
      if ((db_entry.branch == BRANCH_DIRECT_JUMP) || (db_entry.branch == BRANCH_DIRECT_CALL)
          || (((db_entry.branch == BRANCH_CONDITIONAL) || (db_entry.branch == BRANCH_OTHER)) && db_entry.branch_taken == db_entry.branch_prediction)) {
        // clear the branch_mispredicted bit so we don't attempt to resume fetch again at execute
        db_entry.branch_mispredicted = 0;
        // pay misprediction penalty
        this->fetch_resume_time = this->current_time + BRANCH_MISPREDICT_PENALTY;
      }
    }
    // Add to dispatch
    db_entry.ready_time = this->current_time + (this->warmup ? champsim::chrono::clock::duration{} : this->DISPATCH_LATENCY);

    if constexpr (champsim::debug_print) {
      fmt::print("[DECODE] do_decode instr_id: {} time: {}\n", db_entry.instr_id, this->current_time.time_since_epoch() / this->clock_period);
    }
  };

  auto do_dib_hit = [&, this](auto& dib_entry) {
    dib_entry.ready_time = this->current_time + (this->warmup ? champsim::chrono::clock::duration{} : this->DISPATCH_LATENCY);
  };

  std::for_each(decode_buffer_begin, decode_buffer_end, do_decode);
  std::for_each(dib_hit_buffer_begin, dib_hit_buffer_end, do_dib_hit);

  long progress{std::distance(dib_hit_buffer_begin, dib_hit_buffer_end) + std::distance(decode_buffer_begin, decode_buffer_end)};

  std::merge(dib_hit_buffer_begin, dib_hit_buffer_end, decode_buffer_begin, decode_buffer_end, std::back_inserter(DISPATCH_BUFFER),
             ooo_model_instr::program_order);
  DECODE_BUFFER.erase(decode_buffer_begin, decode_buffer_end);
  DIB_HIT_BUFFER.erase(dib_hit_buffer_begin, dib_hit_buffer_end);

  return progress;
}

void O3_CPU::do_dib_update(const ooo_model_instr& instr) { DIB.fill(instr.ip); }

long O3_CPU::dispatch_instruction()
{
  champsim::bandwidth available_dispatch_bandwidth{DISPATCH_WIDTH};

  // dispatch DISPATCH_WIDTH instructions into the ROB
  while (available_dispatch_bandwidth.has_remaining() && !std::empty(DISPATCH_BUFFER) && DISPATCH_BUFFER.front().ready_time <= current_time
         && std::size(ROB) != ROB_SIZE
         && ((std::size_t)std::count_if(std::begin(LQ), std::end(LQ), [](const auto& lq_entry) { return !lq_entry.has_value(); })
             >= std::size(DISPATCH_BUFFER.front().source_memory))
         && ((std::size(DISPATCH_BUFFER.front().destination_memory) + std::size(SQ)) <= SQ_SIZE)) {
    ROB.push_back(std::move(DISPATCH_BUFFER.front()));
    DISPATCH_BUFFER.pop_front();
    do_memory_scheduling(ROB.back());

    available_dispatch_bandwidth.consume();
    ROB.back().ready_time = current_time + (warmup ? champsim::chrono::clock::duration{} : SCHEDULING_LATENCY);
  }

  return available_dispatch_bandwidth.amount_consumed();
}

long O3_CPU::schedule_instruction()
{
  champsim::bandwidth search_bw{SCHEDULER_SIZE};
  int progress{0};
  for (auto rob_it = std::begin(ROB); rob_it != std::end(ROB) && search_bw.has_remaining(); ++rob_it) {
    // if there aren't enough physical registers available for the next instruction, stop scheduling
    unsigned long sources_to_allocate = std::count_if(rob_it->source_registers.begin(), rob_it->source_registers.end(),
                                                      [&alloc = std::as_const(reg_allocator)](auto srcreg) { return !alloc.isAllocated(srcreg); });
    if (reg_allocator.count_free_registers() < (sources_to_allocate + rob_it->destination_registers.size())) {
      break;
    }
    if (!rob_it->scheduled && rob_it->ready_time <= current_time) {
      do_scheduling(*rob_it);
      ++progress;
    }

    if (!rob_it->executed) {
      search_bw.consume();
    }
  }

  return progress;
}

void O3_CPU::do_scheduling(ooo_model_instr& instr)
{
  // Mark register dependencies
  for (auto& src_reg : instr.source_registers) {
    // rename source register
    src_reg = reg_allocator.rename_src_register(src_reg);
  }

  for (auto& dreg : instr.destination_registers) {
    // rename destination register
    dreg = reg_allocator.rename_dest_register(dreg, instr.instr_id);
  }

  instr.scheduled = true;
}

long O3_CPU::execute_instruction()
{
  champsim::bandwidth exec_bw{EXEC_WIDTH};
  for (auto rob_it = std::begin(ROB); rob_it != std::end(ROB) && exec_bw.has_remaining(); ++rob_it) {
    if (rob_it->scheduled && !rob_it->executed && rob_it->ready_time <= current_time) {
      bool ready = std::all_of(std::begin(rob_it->source_registers), std::end(rob_it->source_registers),
                               [&alloc = std::as_const(reg_allocator)](auto srcreg) { return alloc.isValid(srcreg); });
      if (ready) {
        do_execution(*rob_it);
        exec_bw.consume();
      }
    }
  }

  return exec_bw.amount_consumed();
}

void O3_CPU::do_execution(ooo_model_instr& instr)
{
  instr.executed = true;
  instr.ready_time = current_time + (warmup ? champsim::chrono::clock::duration{} : EXEC_LATENCY);

  // Mark LQ entries as ready to translate
  for (auto& lq_entry : LQ) {
    if (lq_entry.has_value() && lq_entry->instr_id == instr.instr_id) {
      lq_entry->ready_time = current_time + (warmup ? champsim::chrono::clock::duration{} : EXEC_LATENCY);
    }
  }

  // Mark SQ entries as ready to translate
  for (auto& sq_entry : SQ) {
    if (sq_entry.instr_id == instr.instr_id) {
      sq_entry.ready_time = current_time + (warmup ? champsim::chrono::clock::duration{} : EXEC_LATENCY);
    }
  }

  if constexpr (champsim::debug_print) {
    fmt::print("[ROB] {} instr_id: {} ready_time: {}\n", __func__, instr.instr_id, instr.ready_time.time_since_epoch() / clock_period);
  }
}

void O3_CPU::do_memory_scheduling(ooo_model_instr& instr)
{
  // load
  for (auto& smem : instr.source_memory) {
    auto q_entry = std::find_if_not(std::begin(LQ), std::end(LQ), [](const auto& lq_entry) { return lq_entry.has_value(); });
    assert(q_entry != std::end(LQ));
    q_entry->emplace(smem, instr.instr_id, instr.ip, instr.asid); // add it to the load queue

    // Check for forwarding
    auto sq_it = std::max_element(std::begin(SQ), std::end(SQ), [smem](const auto& lhs, const auto& rhs) {
      return lhs.virtual_address != smem || (rhs.virtual_address == smem && LSQ_ENTRY::program_order(lhs, rhs));
    });
    if (sq_it != std::end(SQ) && sq_it->virtual_address == smem) {
      if (sq_it->fetch_issued) { // Store already executed
        (*q_entry)->finish(instr);
        q_entry->reset();
      } else {
        assert(sq_it->instr_id < instr.instr_id);      // The found SQ entry is a prior store
        sq_it->lq_depend_on_me.emplace_back(*q_entry); // Forward the load when the store finishes
        (*q_entry)->producer_id = sq_it->instr_id;     // The load waits on the store to finish

        if constexpr (champsim::debug_print) {
          fmt::print("[DISPATCH] {} instr_id: {} waits on: {}\n", __func__, instr.instr_id, sq_it->instr_id);
        }
      }
    }
  }

  // store
  for (auto& dmem : instr.destination_memory) {
    SQ.emplace_back(dmem, instr.instr_id, instr.ip, instr.asid); // add it to the store queue
  }

  if constexpr (champsim::debug_print) {
    fmt::print("[DISPATCH] {} instr_id: {} loads: {} stores: {} cycle: {}\n", __func__, instr.instr_id, std::size(instr.source_memory),
               std::size(instr.destination_memory), current_time.time_since_epoch() / clock_period);
  }
}

long O3_CPU::operate_lsq()
{
  champsim::bandwidth store_bw{SQ_WIDTH};

  const auto complete_id = std::empty(ROB) ? std::numeric_limits<uint64_t>::max() : ROB.front().instr_id;
  auto do_complete = [time = current_time, finished = LSQ_ENTRY::precedes(complete_id), this](const auto& x) {
    return finished(x) && x.ready_time <= time && this->do_complete_store(x);
  };

  auto unfetched_begin = std::partition_point(std::begin(SQ), std::end(SQ), [](const auto& x) { return x.fetch_issued; });
  auto [fetch_begin, fetch_end] =
      champsim::get_span_p(unfetched_begin, std::end(SQ), store_bw, [time = current_time](const auto& x) { return !x.fetch_issued && x.ready_time <= time; });
  store_bw.consume(std::distance(fetch_begin, fetch_end));
  std::for_each(fetch_begin, fetch_end, [time = current_time, this](auto& sq_entry) {
    this->do_finish_store(sq_entry);
    sq_entry.fetch_issued = true;
    sq_entry.ready_time = time;
  });

  auto [complete_begin, complete_end] = champsim::get_span_p(std::cbegin(SQ), std::cend(SQ), store_bw, do_complete);
  store_bw.consume(std::distance(complete_begin, complete_end));
  SQ.erase(complete_begin, complete_end);

  champsim::bandwidth load_bw{LQ_WIDTH};

  for (auto& lq_entry : LQ) {
    if (load_bw.has_remaining() && lq_entry.has_value() && lq_entry->producer_id == std::numeric_limits<uint64_t>::max() && !lq_entry->fetch_issued
        && lq_entry->ready_time < current_time) {
      auto success = execute_load(*lq_entry);
      if (success) {
        load_bw.consume();
        lq_entry->fetch_issued = true;
      }
    }
  }

  return store_bw.amount_consumed() + load_bw.amount_consumed();
}

void O3_CPU::do_finish_store(const LSQ_ENTRY& sq_entry)
{
  if constexpr (champsim::debug_print) {
    fmt::print("[SQ] {} instr_id: {} vaddr: {:x}\n", __func__, sq_entry.instr_id, sq_entry.virtual_address);
  }

  sq_entry.finish(std::begin(ROB), std::end(ROB));

  // Release dependent loads
  for (std::optional<LSQ_ENTRY>& dependent : sq_entry.lq_depend_on_me) {
    assert(dependent.has_value()); // LQ entry is still allocated
    assert(dependent->producer_id == sq_entry.instr_id);

    dependent->finish(std::begin(ROB), std::end(ROB));
    dependent.reset();
  }
}

bool O3_CPU::do_complete_store(const LSQ_ENTRY& sq_entry)
{
  CacheBus::request_type data_packet;
  data_packet.v_address = sq_entry.virtual_address;
  data_packet.instr_id = sq_entry.instr_id;
  data_packet.ip = sq_entry.ip;

  if constexpr (champsim::debug_print) {
    fmt::print("[SQ] {} instr_id: {} vaddr: {:x}\n", __func__, data_packet.instr_id, data_packet.v_address);
  }

  return L1D_bus.issue_write(data_packet);
}

bool O3_CPU::execute_load(const LSQ_ENTRY& lq_entry)
{
  CacheBus::request_type data_packet;
  data_packet.v_address = lq_entry.virtual_address;
  data_packet.instr_id = lq_entry.instr_id;
  data_packet.ip = lq_entry.ip;

  if constexpr (champsim::debug_print) {
    fmt::print("[LQ] {} instr_id: {} vaddr: {:#x}\n", __func__, data_packet.instr_id, data_packet.v_address);
  }

  return L1D_bus.issue_read(data_packet);
}

void O3_CPU::do_complete_execution(ooo_model_instr& instr)
{
  for (auto dreg : instr.destination_registers) {
    // mark physical register's data as valid
    reg_allocator.complete_dest_register(dreg);
  }

  instr.completed = true;

  if (instr.branch_mispredicted) {
    fetch_resume_time = current_time + BRANCH_MISPREDICT_PENALTY;
  }
}

long O3_CPU::complete_inflight_instruction()
{
  // update ROB entries with completed executions
  champsim::bandwidth complete_bw{EXEC_WIDTH};
  for (auto rob_it = std::begin(ROB); rob_it != std::end(ROB) && complete_bw.has_remaining(); ++rob_it) {
    if (rob_it->executed && !rob_it->completed && (rob_it->ready_time <= current_time) && rob_it->completed_mem_ops == rob_it->num_mem_ops()) {
      do_complete_execution(*rob_it);
      complete_bw.consume();
    }
  }

  return complete_bw.amount_consumed();
}

long O3_CPU::handle_memory_return()
{
  long progress{0};

  for (champsim::bandwidth l1i_bw{FETCH_WIDTH}, to_read{L1I_BANDWIDTH};
       l1i_bw.has_remaining() && to_read.has_remaining() && !L1I_bus.lower_level->returned.empty(); to_read.consume()) {
    auto& l1i_entry = L1I_bus.lower_level->returned.front();

    while (l1i_bw.has_remaining() && !l1i_entry.instr_depend_on_me.empty()) {
      auto fetched = std::find_if(std::begin(IFETCH_BUFFER), std::end(IFETCH_BUFFER), ooo_model_instr::matches_id(l1i_entry.instr_depend_on_me.front()));
      if (fetched != std::end(IFETCH_BUFFER) && champsim::block_number{fetched->ip} == champsim::block_number{l1i_entry.v_address} && fetched->fetch_issued) {
        fetched->fetch_completed = true;
        l1i_bw.consume();
        ++progress;

        if constexpr (champsim::debug_print) {
          fmt::print("[IFETCH] {} instr_id: {} fetch completed\n", __func__, fetched->instr_id);
        }
      }

      l1i_entry.instr_depend_on_me.erase(std::begin(l1i_entry.instr_depend_on_me));
    }

    // remove this entry if we have serviced all of its instructions
    if (l1i_entry.instr_depend_on_me.empty()) {
      L1I_bus.lower_level->returned.pop_front();
      ++progress;
    }
  }

  auto l1d_it = std::begin(L1D_bus.lower_level->returned);
  for (champsim::bandwidth l1d_bw{L1D_BANDWIDTH}; l1d_bw.has_remaining() && l1d_it != std::end(L1D_bus.lower_level->returned); l1d_bw.consume(), ++l1d_it) {
    for (auto& lq_entry : LQ) {
      if (lq_entry.has_value() && lq_entry->fetch_issued && champsim::block_number{lq_entry->virtual_address} == champsim::block_number{l1d_it->v_address}) {
        lq_entry->finish(std::begin(ROB), std::end(ROB));
        lq_entry.reset();
        ++progress;
      }
    }
    ++progress;
  }
  L1D_bus.lower_level->returned.erase(std::begin(L1D_bus.lower_level->returned), l1d_it);

  return progress;
}

long O3_CPU::retire_rob()
{
  auto [retire_begin, retire_end] =
      champsim::get_span_p(std::cbegin(ROB), std::cend(ROB), champsim::bandwidth{RETIRE_WIDTH}, [](const auto& x) { return x.completed; });
  assert(std::distance(retire_begin, retire_end) >= 0); // end succeeds begin
  if constexpr (champsim::debug_print) {
    std::for_each(retire_begin, retire_end, [cycle = current_time.time_since_epoch() / clock_period](const auto& x) {
      fmt::print("[ROB] retire_rob instr_id: {} is retired cycle: {}\n", x.instr_id, cycle);
    });
  }

  // commit register writes to backend RAT
  // and recycle the old physical registers
  for (auto rob_it = retire_begin; rob_it != retire_end; ++rob_it) {
    for (auto dreg : rob_it->destination_registers) {
      reg_allocator.retire_dest_register(dreg);
    }
  }

  auto retire_count = std::distance(retire_begin, retire_end);
  num_retired += retire_count;
  ROB.erase(retire_begin, retire_end);

  return retire_count;
}

void O3_CPU::impl_initialize_branch_predictor() const { branch_module_pimpl->impl_initialize_branch_predictor(); }

void O3_CPU::impl_last_branch_result(champsim::address ip, champsim::address target, bool taken, uint8_t branch_type) const
{
  branch_module_pimpl->impl_last_branch_result(ip, target, taken, branch_type);
}

bool O3_CPU::impl_predict_branch(champsim::address ip, champsim::address predicted_target, bool always_taken, uint8_t branch_type) const
{
  return branch_module_pimpl->impl_predict_branch(ip, predicted_target, always_taken, branch_type);
}

void O3_CPU::impl_initialize_btb() const { btb_module_pimpl->impl_initialize_btb(); }

void O3_CPU::impl_update_btb(champsim::address ip, champsim::address predicted_target, bool taken, uint8_t branch_type) const
{
  btb_module_pimpl->impl_update_btb(ip, predicted_target, taken, branch_type);
}

std::pair<champsim::address, bool> O3_CPU::impl_btb_prediction(champsim::address ip, uint8_t branch_type) const
{
  return btb_module_pimpl->impl_btb_prediction(ip, branch_type);
}

// LCOV_EXCL_START Exclude the following function from LCOV
void O3_CPU::print_deadlock()
{
  fmt::print("DEADLOCK! CPU {} cycle {}\n", cpu, current_time.time_since_epoch() / clock_period);

  auto instr_pack = [period = clock_period, this](const auto& entry) {
    return std::tuple{entry.instr_id,
                      entry.fetch_issued,
                      entry.fetch_completed,
                      entry.scheduled,
                      entry.executed,
                      entry.completed,
                      reg_allocator.count_reg_dependencies(entry),
                      entry.num_mem_ops() - entry.completed_mem_ops,
                      entry.ready_time.time_since_epoch() / period};
  };
  std::string_view instr_fmt{
      "instr_id: {} fetch_issued: {} fetch_completed: {} scheduled: {} executed: {} completed: {} num_reg_dependent: {} num_mem_ops: {} event: {}"};
  champsim::range_print_deadlock(IFETCH_BUFFER, "cpu" + std::to_string(cpu) + "_IFETCH", instr_fmt, instr_pack);
  champsim::range_print_deadlock(DECODE_BUFFER, "cpu" + std::to_string(cpu) + "_DECODE", instr_fmt, instr_pack);
  champsim::range_print_deadlock(DISPATCH_BUFFER, "cpu" + std::to_string(cpu) + "_DISPATCH", instr_fmt, instr_pack);
  champsim::range_print_deadlock(ROB, "cpu" + std::to_string(cpu) + "_ROB", instr_fmt, instr_pack);

  // print occupied physical registers
  reg_allocator.print_deadlock();

  // print LQ entry
  auto lq_pack = [period = clock_period](const auto& entry) {
    std::string depend_id{"-"};
    if (entry->producer_id != std::numeric_limits<uint64_t>::max()) {
      depend_id = std::to_string(entry->producer_id);
    }
    return std::tuple{entry->instr_id, entry->virtual_address, entry->fetch_issued, entry->ready_time.time_since_epoch() / period, depend_id};
  };
  std::string_view lq_fmt{"instr_id: {} address: {} fetch_issued: {} event_cycle: {} waits on {}"};

  auto sq_pack = [period = clock_period](const auto& entry) {
    std::vector<uint64_t> depend_ids;
    std::transform(std::begin(entry.lq_depend_on_me), std::end(entry.lq_depend_on_me), std::back_inserter(depend_ids),
                   [](const std::optional<LSQ_ENTRY>& lq_entry) { return lq_entry->producer_id; });
    return std::tuple{entry.instr_id, entry.virtual_address, entry.fetch_issued, entry.ready_time.time_since_epoch() / period, depend_ids};
  };
  std::string_view sq_fmt{"instr_id: {} address: {} fetch_issued: {} event_cycle: {} LQ waiting: {}"};
  champsim::range_print_deadlock(LQ, "cpu" + std::to_string(cpu) + "_LQ", lq_fmt, lq_pack);
  champsim::range_print_deadlock(SQ, "cpu" + std::to_string(cpu) + "_SQ", sq_fmt, sq_pack);
}
// LCOV_EXCL_STOP

LSQ_ENTRY::LSQ_ENTRY(champsim::address addr, champsim::program_ordered<LSQ_ENTRY>::id_type id, champsim::address local_ip, std::array<uint8_t, 2> local_asid)
    : champsim::program_ordered<LSQ_ENTRY>{id}, virtual_address(addr), ip(local_ip), asid(local_asid)
{
}

void LSQ_ENTRY::finish(std::deque<ooo_model_instr>::iterator begin, std::deque<ooo_model_instr>::iterator end) const
{
  auto rob_entry = std::partition_point(begin, end, ooo_model_instr::precedes(this->instr_id));
  assert(rob_entry != end);
  finish(*rob_entry);
}

void LSQ_ENTRY::finish(ooo_model_instr& rob_entry) const
{
  assert(rob_entry.instr_id == this->instr_id);

  ++rob_entry.completed_mem_ops;
  assert(rob_entry.completed_mem_ops <= rob_entry.num_mem_ops());

  if constexpr (champsim::debug_print) {
    fmt::print("[LSQ] {} instr_id: {} full_address: {:#x} remain_mem_ops: {}\n", __func__, instr_id, virtual_address,
               rob_entry.num_mem_ops() - rob_entry.completed_mem_ops);
  }
}

bool CacheBus::issue_read(request_type data_packet)
{
  data_packet.address = data_packet.v_address;
  data_packet.is_translated = false;
  data_packet.cpu = cpu;
  data_packet.type = access_type::LOAD;

  return lower_level->add_rq(data_packet);
}

bool CacheBus::issue_write(request_type data_packet)
{
  data_packet.address = data_packet.v_address;
  data_packet.is_translated = false;
  data_packet.cpu = cpu;
  data_packet.type = access_type::WRITE;
  data_packet.response_requested = false;

  return lower_level->add_wq(data_packet);
}
