#include "ooo_cpu.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <numeric>
#include <utility>
#include <vector>

#include "cache.h"
#include "champsim.h"
#include "instruction.h"
#include "util/span.h"

constexpr uint64_t DEADLOCK_CYCLE = 1000000;

std::tuple<uint64_t, uint64_t, uint64_t> elapsed_time();

void O3_CPU::operate()
{
  instrs_to_read_this_cycle = std::min<std::size_t>(FETCH_WIDTH, IFETCH_BUFFER_SIZE - std::size(IFETCH_BUFFER));

  retire_rob();                    // retire
  complete_inflight_instruction(); // finalize execution
  execute_instruction();           // execute instructions
  schedule_instruction();          // schedule instructions
  handle_memory_return();          // finalize memory transactions
  operate_lsq();                   // execute memory transactions

  dispatch_instruction(); // dispatch
  decode_instruction();   // decode
  promote_to_decode();

  // if we had a branch mispredict, turn fetching back on after the branch
  // mispredict penalty
  if ((fetch_stall == 1) && (current_cycle >= fetch_resume_cycle) && (fetch_resume_cycle != 0)) {
    fetch_stall = 0;
    fetch_resume_cycle = 0;
  }

  fetch_instruction(); // fetch
  check_dib();
  initialize_instruction();

  // heartbeat
  if (show_heartbeat && (num_retired >= next_print_instruction)) {
    auto [elapsed_hour, elapsed_minute, elapsed_second] = elapsed_time();

    auto heartbeat_instr{std::ceil(num_retired - last_heartbeat_instr)};
    auto heartbeat_cycle{std::ceil(current_cycle - last_heartbeat_cycle)};

    auto phase_instr{std::ceil(num_retired - begin_phase_instr)};
    auto phase_cycle{std::ceil(current_cycle - begin_phase_cycle)};

    std::cout << "Heartbeat CPU " << cpu << " instructions: " << num_retired << " cycles: " << current_cycle;
    std::cout << " heartbeat IPC: " << heartbeat_instr / heartbeat_cycle;
    std::cout << " cumulative IPC: " << phase_instr / phase_cycle;
    std::cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << std::endl;
    next_print_instruction += STAT_PRINTING_PERIOD;

    last_heartbeat_instr = num_retired;
    last_heartbeat_cycle = current_cycle;
  }
}

void O3_CPU::initialize()
{
  // BRANCH PREDICTOR & BTB
  impl_branch_predictor_initialize();
  impl_btb_initialize();
}

void O3_CPU::begin_phase()
{
  begin_phase_instr = num_retired;
  begin_phase_cycle = current_cycle;

  // Record where the next phase begins
  stats_type stats;
  stats.name = "CPU " + std::to_string(cpu);
  stats.begin_instrs = num_retired;
  stats.begin_cycles = current_cycle;
  sim_stats.push_back(stats);
}

void O3_CPU::end_phase(unsigned finished_cpu)
{
  // Record where the phase ended (overwrite if this is later)
  sim_stats.back().end_instrs = num_retired;
  sim_stats.back().end_cycles = current_cycle;

  if (finished_cpu == this->cpu) {
    finish_phase_instr = num_retired;
    finish_phase_cycle = current_cycle;

    roi_stats.push_back(sim_stats.back());
  }
}

void O3_CPU::initialize_instruction()
{
  while (fetch_stall == 0 && instrs_to_read_this_cycle > 0 && !std::empty(input_queue)) {
    do_init_instruction(input_queue.front());
    input_queue.pop_front();
  }
}

void O3_CPU::do_init_instruction(ooo_model_instr& arch_instr)
{
  instrs_to_read_this_cycle--;

  arch_instr.instr_id = instr_unique_id;

  bool writes_sp = std::count(std::begin(arch_instr.destination_registers), std::end(arch_instr.destination_registers), champsim::REG_STACK_POINTER);
  bool writes_ip = std::count(std::begin(arch_instr.destination_registers), std::end(arch_instr.destination_registers), champsim::REG_INSTRUCTION_POINTER);
  bool reads_sp = std::count(std::begin(arch_instr.source_registers), std::end(arch_instr.source_registers), champsim::REG_STACK_POINTER);
  bool reads_flags = std::count(std::begin(arch_instr.source_registers), std::end(arch_instr.source_registers), champsim::REG_FLAGS);
  bool reads_ip = std::count(std::begin(arch_instr.source_registers), std::end(arch_instr.source_registers), champsim::REG_INSTRUCTION_POINTER);
  bool reads_other = std::count_if(std::begin(arch_instr.source_registers), std::end(arch_instr.source_registers), [](uint8_t r) {
    return r != champsim::REG_STACK_POINTER && r != champsim::REG_FLAGS && r != champsim::REG_INSTRUCTION_POINTER;
  });

  // determine what kind of branch this is, if any
  if (!reads_sp && !reads_flags && writes_ip && !reads_other) {
    // direct jump
    arch_instr.is_branch = 1;
    arch_instr.branch_taken = 1;
    arch_instr.branch_type = BRANCH_DIRECT_JUMP;
  } else if (!reads_sp && !reads_flags && writes_ip && reads_other) {
    // indirect branch
    arch_instr.is_branch = 1;
    arch_instr.branch_taken = 1;
    arch_instr.branch_type = BRANCH_INDIRECT;
  } else if (!reads_sp && reads_ip && !writes_sp && writes_ip && reads_flags && !reads_other) {
    // conditional branch
    arch_instr.is_branch = 1;
    arch_instr.branch_taken = arch_instr.branch_taken; // don't change this
    arch_instr.branch_type = BRANCH_CONDITIONAL;
  } else if (reads_sp && reads_ip && writes_sp && writes_ip && !reads_flags && !reads_other) {
    // direct call
    arch_instr.is_branch = 1;
    arch_instr.branch_taken = 1;
    arch_instr.branch_type = BRANCH_DIRECT_CALL;
  } else if (reads_sp && reads_ip && writes_sp && writes_ip && !reads_flags && reads_other) {
    // indirect call
    arch_instr.is_branch = 1;
    arch_instr.branch_taken = 1;
    arch_instr.branch_type = BRANCH_INDIRECT_CALL;
  } else if (reads_sp && !reads_ip && writes_sp && writes_ip) {
    // return
    arch_instr.is_branch = 1;
    arch_instr.branch_taken = 1;
    arch_instr.branch_type = BRANCH_RETURN;
  } else if (writes_ip) {
    // some other branch type that doesn't fit the above categories
    arch_instr.is_branch = 1;
    arch_instr.branch_taken = arch_instr.branch_taken; // don't change this
    arch_instr.branch_type = BRANCH_OTHER;
  } else {
    assert(!arch_instr.is_branch);
    assert(arch_instr.branch_type == NOT_BRANCH);
    arch_instr.branch_taken = 0;
  }

  if (arch_instr.branch_taken != 1) {
    // clear the branch target for non-taken instructions
    arch_instr.branch_target = champsim::address{};
  }

  sim_stats.back().total_branch_types[arch_instr.branch_type]++;

  // Stack Pointer Folding
  // The exact, true value of the stack pointer for any given instruction can
  // usually be determined immediately after the instruction is decoded without
  // waiting for the stack pointer's dependency chain to be resolved.
  // We're doing it here because we already have writes_sp and reads_other
  // handy, and in ChampSim it doesn't matter where before execution you do it.
  if (writes_sp) {
    // Avoid creating register dependencies on the stack pointer for calls,
    // returns, pushes, and pops, but not for variable-sized changes in the
    // stack pointer position. reads_other indicates that the stack pointer is
    // being changed by a variable amount, which can't be determined before
    // execution.
    if ((arch_instr.is_branch != 0) || !(std::empty(arch_instr.destination_memory) && std::empty(arch_instr.source_memory)) || (!reads_other)) {
      auto nonsp_end = std::remove(std::begin(arch_instr.destination_registers), std::end(arch_instr.destination_registers), champsim::REG_STACK_POINTER);
      arch_instr.destination_registers.erase(nonsp_end, std::end(arch_instr.destination_registers));
    }
  }

  // handle branch prediction for all instructions as at this point we do not know if the instruction is a branch
  sim_stats.back().total_branch_types[arch_instr.branch_type]++;
  auto [predicted_branch_target, always_taken] = impl_btb_prediction(arch_instr.ip);
  arch_instr.branch_prediction = impl_predict_branch(arch_instr.ip);
  if (always_taken) {
    arch_instr.branch_prediction = true;
  }
  if (arch_instr.branch_prediction == 0) {
    predicted_branch_target = champsim::address{};
  }

  if (arch_instr.is_branch) {
    if constexpr (champsim::debug_print) {
      std::cout << "[BRANCH] instr_id: " << instr_unique_id << " ip: " << std::hex << arch_instr.ip << std::dec << " taken: " << +arch_instr.branch_taken
                << std::endl;
    }

    // call code prefetcher every time the branch predictor is used
    static_cast<CACHE*>(L1I_bus.lower_level)->impl_prefetcher_branch_operate(arch_instr.ip, arch_instr.branch_type, predicted_branch_target);

    if (predicted_branch_target != arch_instr.branch_target
        || (arch_instr.branch_type == BRANCH_CONDITIONAL
            && arch_instr.branch_taken != arch_instr.branch_prediction)) { // conditional branches are re-evaluated at decode when the target is computed
      sim_stats.back().total_rob_occupancy_at_branch_mispredict += std::size(ROB);
      sim_stats.back().branch_type_misses[arch_instr.branch_type]++;
      if (!warmup) {
        fetch_stall = 1;
        instrs_to_read_this_cycle = 0;
        arch_instr.branch_mispredicted = 1;
      }
    } else {
      // if correctly predicted taken, then we can't fetch anymore instructions this cycle
      if (arch_instr.branch_taken == 1) {
        instrs_to_read_this_cycle = 0;
      }
    }

    impl_update_btb(arch_instr.ip, arch_instr.branch_target, arch_instr.branch_taken, arch_instr.branch_type);
    impl_last_branch_result(arch_instr.ip, arch_instr.branch_target, arch_instr.branch_taken, arch_instr.branch_type);
  }

  arch_instr.event_cycle = current_cycle;

  // fast warmup eliminates register dependencies between instructions
  // branch predictor, cache contents, and prefetchers are still warmed up
  if (warmup) {
    arch_instr.source_registers.clear();
    arch_instr.destination_registers.clear();
  }

  // Add to IFETCH_BUFFER
  IFETCH_BUFFER.push_back(arch_instr);

  instr_unique_id++;
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
  auto to_read = L1I_BANDWIDTH;
  auto l1i_req_begin = std::find_if(std::begin(IFETCH_BUFFER), std::end(IFETCH_BUFFER), fetch_ready);
  while (to_read > 0 && l1i_req_begin != std::end(IFETCH_BUFFER)) {
    // Find the chunk of instructions in the block
    auto no_match_ip = [find_ip = champsim::block_number{l1i_req_begin->ip}](const ooo_model_instr& x) {
      return find_ip != champsim::block_number{x.ip};
    };
    auto l1i_req_end = std::find_if(l1i_req_begin, std::end(IFETCH_BUFFER), no_match_ip);

    // Issue to L1I
    auto success = do_fetch_instruction(l1i_req_begin, l1i_req_end);
    if (success) {
      for (auto it = l1i_req_begin; it != l1i_req_end; ++it)
        it->fetched = INFLIGHT;
      break;
    }

    --to_read;
    l1i_req_begin = std::find_if(l1i_req_end, std::end(IFETCH_BUFFER), fetch_ready);
  }
}

bool O3_CPU::do_fetch_instruction(std::deque<ooo_model_instr>::iterator begin, std::deque<ooo_model_instr>::iterator end)
{
  PACKET fetch_packet;
  fetch_packet.v_address = begin->ip;
  fetch_packet.instr_id = begin->instr_id;
  fetch_packet.ip = begin->ip;
  fetch_packet.instr_depend_on_me = {begin, end};

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

  // check for deadlock
  if (!std::empty(IFETCH_BUFFER) && (IFETCH_BUFFER.front().event_cycle + DEADLOCK_CYCLE) <= current_cycle)
    throw champsim::deadlock{cpu};
}

void O3_CPU::decode_instruction()
{
  auto available_decode_bandwidth = std::min<long>(DECODE_WIDTH, DISPATCH_BUFFER_SIZE - std::size(DISPATCH_BUFFER));
  auto [window_begin, window_end] = champsim::get_span_p(std::begin(DECODE_BUFFER), std::end(DECODE_BUFFER), available_decode_bandwidth,
                                                         [cycle = current_cycle](const auto& x) { return x.event_cycle <= cycle; });

  // Send decoded instructions to dispatch
  std::for_each(window_begin, window_end, [&, this](auto& db_entry) {
    this->do_dib_update(db_entry);

    // Resume fetch
    if (db_entry.branch_mispredicted) {
      // These branches detect the misprediction at decode
      if ((db_entry.branch_type == BRANCH_DIRECT_JUMP) || (db_entry.branch_type == BRANCH_DIRECT_CALL)
          || (db_entry.branch_type == BRANCH_CONDITIONAL && db_entry.branch_taken == db_entry.branch_prediction)) {
        // clear the branch_mispredicted bit so we don't attempt to resume fetch again at execute
        db_entry.branch_mispredicted = 0;
        // pay misprediction penalty
        this->fetch_resume_cycle = this->current_cycle + BRANCH_MISPREDICT_PENALTY;
      }
    }

    // Add to dispatch
    db_entry.event_cycle = this->current_cycle + (this->warmup ? 0 : this->DISPATCH_LATENCY);
  });

  std::move(window_begin, window_end, std::back_inserter(DISPATCH_BUFFER));
  DECODE_BUFFER.erase(window_begin, window_end);

  // check for deadlock
  if (!std::empty(DECODE_BUFFER) && (DECODE_BUFFER.front().event_cycle + DEADLOCK_CYCLE) <= current_cycle)
    throw champsim::deadlock{cpu};
}

void O3_CPU::do_dib_update(const ooo_model_instr& instr) { DIB.fill(instr.ip); }

void O3_CPU::dispatch_instruction()
{
  std::size_t available_dispatch_bandwidth = DISPATCH_WIDTH;

  // dispatch DISPATCH_WIDTH instructions into the ROB
  while (available_dispatch_bandwidth > 0 && !std::empty(DISPATCH_BUFFER) && DISPATCH_BUFFER.front().event_cycle < current_cycle && std::size(ROB) != ROB_SIZE
         && ((std::size_t)std::count_if(std::begin(LQ), std::end(LQ), [](const auto& lq_entry){ return !lq_entry.has_value(); })
             >= std::size(DISPATCH_BUFFER.front().source_memory))
         && ((std::size(DISPATCH_BUFFER.front().destination_memory) + std::size(SQ)) <= SQ_SIZE)) {
    ROB.push_back(std::move(DISPATCH_BUFFER.front()));
    DISPATCH_BUFFER.pop_front();
    do_memory_scheduling(ROB.back());

    available_dispatch_bandwidth--;
  }

  // check for deadlock
  if (!std::empty(DISPATCH_BUFFER) && (DISPATCH_BUFFER.front().event_cycle + DEADLOCK_CYCLE) <= current_cycle)
    throw champsim::deadlock{cpu};
}

void O3_CPU::schedule_instruction()
{
  auto search_bw = SCHEDULER_SIZE;
  for (auto rob_it = std::begin(ROB); rob_it != std::end(ROB) && search_bw > 0; ++rob_it) {
    if (rob_it->scheduled == 0)
      do_scheduling(*rob_it);

    if (rob_it->executed == 0)
      --search_bw;
  }
}

void O3_CPU::do_scheduling(ooo_model_instr& instr)
{
  // Mark register dependencies
  for (auto src_reg : instr.source_registers) {
    if (!std::empty(reg_producers[src_reg])) {
      ooo_model_instr& prior = reg_producers[src_reg].back();
      if (prior.registers_instrs_depend_on_me.empty() || prior.registers_instrs_depend_on_me.back().get().instr_id != instr.instr_id) {
        prior.registers_instrs_depend_on_me.push_back(instr);
        instr.num_reg_dependent++;
      }
    }
  }

  for (auto dreg : instr.destination_registers) {
    auto begin = std::begin(reg_producers[dreg]);
    auto end = std::end(reg_producers[dreg]);
    auto ins = std::lower_bound(begin, end, instr, [](const ooo_model_instr& lhs, const ooo_model_instr& rhs) { return lhs.instr_id < rhs.instr_id; });
    reg_producers[dreg].insert(ins, std::ref(instr));
  }

  instr.scheduled = COMPLETED;
  instr.event_cycle = current_cycle + (warmup ? 0 : SCHEDULING_LATENCY);
}

void O3_CPU::execute_instruction()
{
  auto exec_bw = EXEC_WIDTH;
  for (auto rob_it = std::begin(ROB); rob_it != std::end(ROB) && exec_bw > 0; ++rob_it) {
    if (rob_it->scheduled == COMPLETED && rob_it->executed == 0 && rob_it->num_reg_dependent == 0 && rob_it->event_cycle <= current_cycle) {
      do_execution(*rob_it);
      --exec_bw;
    }
  }
}

void O3_CPU::do_execution(ooo_model_instr& rob_entry)
{
  rob_entry.executed = INFLIGHT;
  rob_entry.event_cycle = current_cycle + (warmup ? 0 : EXEC_LATENCY);

  // Mark LQ entries as ready to translate
  for (auto& lq_entry : LQ)
    if (lq_entry.has_value() && lq_entry->instr_id == rob_entry.instr_id)
      lq_entry->event_cycle = current_cycle + (warmup ? 0 : EXEC_LATENCY);

  // Mark SQ entries as ready to translate
  for (auto& sq_entry : SQ)
    if (sq_entry.instr_id == rob_entry.instr_id)
      sq_entry.event_cycle = current_cycle + (warmup ? 0 : EXEC_LATENCY);

  if constexpr (champsim::debug_print) {
    std::cout << "[ROB] " << __func__ << " instr_id: " << rob_entry.instr_id << " event_cycle: " << rob_entry.event_cycle << std::endl;
  }
}

void O3_CPU::do_memory_scheduling(ooo_model_instr& instr)
{
  // load
  for (auto& smem : instr.source_memory) {
    auto q_entry = std::find_if_not(std::begin(LQ), std::end(LQ), [](const auto& lq_entry){ return lq_entry.has_value(); });
    assert(q_entry != std::end(LQ));
    q_entry->emplace(LSQ_ENTRY{instr.instr_id,
                               smem,
                               instr.ip,
                               std::numeric_limits<uint64_t>::max(),
                               {instr.asid[0], instr.asid[1]},
                               false,
                               std::numeric_limits<uint64_t>::max(),
                               {}}); // add it to the load queue

    // Check for forwarding
    auto sq_it = std::max_element(std::begin(SQ), std::end(SQ), [smem](const auto& lhs, const auto& rhs) {
      return lhs.virtual_address != smem || (rhs.virtual_address == smem && lhs.instr_id < rhs.instr_id);
    });
    if (sq_it != std::end(SQ) && sq_it->virtual_address == smem) {
      if (sq_it->fetch_issued) { // Store already executed
        q_entry->reset();
        ++instr.completed_mem_ops;

        if constexpr (champsim::debug_print)
          std::cout << "[DISPATCH] " << __func__ << " instr_id: " << instr.instr_id << " forwards from " << sq_it->instr_id << std::endl;
      } else {
        assert(sq_it->instr_id < instr.instr_id);   // The found SQ entry is a prior store
        sq_it->lq_depend_on_me.push_back(*q_entry); // Forward the load when the store finishes
        (*q_entry)->producer_id = sq_it->instr_id;  // The load waits on the store to finish

        if constexpr (champsim::debug_print)
          std::cout << "[DISPATCH] " << __func__ << " instr_id: " << instr.instr_id << " waits on " << sq_it->instr_id << std::endl;
      }
    }
  }

  // store
  for (auto& dmem : instr.destination_memory)
    SQ.push_back({instr.instr_id,
                  dmem,
                  instr.ip,
                  std::numeric_limits<uint64_t>::max(),
                  {instr.asid[0], instr.asid[1]},
                  false,
                  std::numeric_limits<uint64_t>::max(),
                  {}}); // add it to the store queue

  if constexpr (champsim::debug_print) {
    std::cout << "[DISPATCH] " << __func__ << " instr_id: " << instr.instr_id << " loads: " << std::size(instr.source_memory)
              << " stores: " << std::size(instr.destination_memory) << std::endl;
  }
}

void O3_CPU::operate_lsq()
{
  auto store_bw = SQ_WIDTH;

  const auto complete_id = std::empty(ROB) ? std::numeric_limits<uint64_t>::max() : ROB.front().instr_id;
  auto do_complete = [cycle = current_cycle, complete_id, this](const auto& x) {
    return x.instr_id < complete_id && x.event_cycle <= cycle && this->do_complete_store(x);
  };

  auto unfetched_begin = std::partition_point(std::begin(SQ), std::end(SQ), [](const auto& x) { return x.fetch_issued; });
  auto [fetch_begin, fetch_end] = champsim::get_span_p(unfetched_begin, std::end(SQ), store_bw,
                                                       [cycle = current_cycle](const auto& x) { return !x.fetch_issued && x.event_cycle <= cycle; });
  store_bw -= std::distance(fetch_begin, fetch_end);
  std::for_each(fetch_begin, fetch_end, [cycle = current_cycle, this](auto& sq_entry) {
    this->do_finish_store(sq_entry);
    sq_entry.fetch_issued = true;
    sq_entry.event_cycle = cycle;
  });

  auto [complete_begin, complete_end] = champsim::get_span_p(std::cbegin(SQ), std::cend(SQ), store_bw, do_complete);
  store_bw -= std::distance(complete_begin, complete_end);
  SQ.erase(complete_begin, complete_end);

  auto load_bw = LQ_WIDTH;

  for (auto& lq_entry : LQ) {
    if (load_bw > 0 && lq_entry.has_value() && lq_entry->producer_id == std::numeric_limits<uint64_t>::max() && !lq_entry->fetch_issued
        && lq_entry->event_cycle < current_cycle) {
      auto success = execute_load(*lq_entry);
      if (success) {
        --load_bw;
        lq_entry->fetch_issued = true;
      }
    }
  }
}

void O3_CPU::do_finish_store(const LSQ_ENTRY& sq_entry)
{
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
  PACKET data_packet;
  data_packet.v_address = sq_entry.virtual_address;
  data_packet.instr_id = sq_entry.instr_id;
  data_packet.ip = sq_entry.ip;

  if constexpr (champsim::debug_print) {
    std::cout << "[SQ] " << __func__ << " instr_id: " << sq_entry.instr_id << std::endl;
  }

  return L1D_bus.issue_write(data_packet);
}

bool O3_CPU::execute_load(const LSQ_ENTRY& lq_entry)
{
  PACKET data_packet;
  data_packet.v_address = lq_entry.virtual_address;
  data_packet.instr_id = lq_entry.instr_id;
  data_packet.ip = lq_entry.ip;

  if constexpr (champsim::debug_print) {
    std::cout << "[LQ] " << __func__ << " instr_id: " << lq_entry.instr_id << std::endl;
  }

  return L1D_bus.issue_read(data_packet);
}

void O3_CPU::do_complete_execution(ooo_model_instr& instr)
{
  for (auto dreg : instr.destination_registers) {
    auto begin = std::begin(reg_producers[dreg]);
    auto end = std::end(reg_producers[dreg]);
    auto elem = std::find_if(begin, end, [id = instr.instr_id](ooo_model_instr& x) { return x.instr_id == id; });
    assert(elem != end);
    reg_producers[dreg].erase(elem);
  }

  instr.executed = COMPLETED;

  for (ooo_model_instr& dependent : instr.registers_instrs_depend_on_me) {
    dependent.num_reg_dependent--;
    assert(dependent.num_reg_dependent >= 0);

    if (dependent.num_reg_dependent == 0)
      dependent.scheduled = COMPLETED;
  }

  if (instr.branch_mispredicted)
    fetch_resume_cycle = current_cycle + BRANCH_MISPREDICT_PENALTY;
}

void O3_CPU::complete_inflight_instruction()
{
  // update ROB entries with completed executions
  auto complete_bw = EXEC_WIDTH;
  for (auto rob_it = std::begin(ROB); rob_it != std::end(ROB) && complete_bw > 0; ++rob_it) {
    if ((rob_it->executed == INFLIGHT) && (rob_it->event_cycle <= current_cycle) && rob_it->completed_mem_ops == rob_it->num_mem_ops()) {
      do_complete_execution(*rob_it);
      --complete_bw;
    }
  }
}

void O3_CPU::handle_memory_return()
{
  for (auto l1i_bw = FETCH_WIDTH, to_read = L1I_BANDWIDTH; l1i_bw > 0 && to_read > 0 && !L1I_bus.PROCESSED.empty(); --to_read) {
    PACKET& l1i_entry = L1I_bus.PROCESSED.front();

    while (l1i_bw > 0 && !l1i_entry.instr_depend_on_me.empty()) {
      ooo_model_instr& fetched = l1i_entry.instr_depend_on_me.front();
      if (champsim::block_number{fetched.ip} == champsim::block_number{l1i_entry.v_address} && fetched.fetched != 0) {
        fetched.fetched = COMPLETED;
        --l1i_bw;

        if constexpr (champsim::debug_print) {
          std::cout << "[IFETCH] " << __func__ << " instr_id: " << fetched.instr_id << " fetch completed" << std::endl;
        }
      }

      l1i_entry.instr_depend_on_me.erase(std::begin(l1i_entry.instr_depend_on_me));
    }

    // remove this entry if we have serviced all of its instructions
    if (l1i_entry.instr_depend_on_me.empty())
      L1I_bus.PROCESSED.pop_front();
  }

  auto l1d_it = std::begin(L1D_bus.PROCESSED);
  for (auto l1d_bw = L1D_BANDWIDTH; l1d_bw > 0 && l1d_it != std::end(L1D_bus.PROCESSED); --l1d_bw, ++l1d_it) {
    for (auto& lq_entry : LQ) {
      if (lq_entry.has_value() && lq_entry->fetch_issued && champsim::block_number{lq_entry->virtual_address} == champsim::block_number{l1d_it->v_address}) {
        lq_entry->finish(std::begin(ROB), std::end(ROB));
        lq_entry.reset();
      }
    }
  }
  L1D_bus.PROCESSED.erase(std::begin(L1D_bus.PROCESSED), l1d_it);
}

void O3_CPU::retire_rob()
{
  auto [retire_begin, retire_end] = champsim::get_span_p(std::cbegin(ROB), std::cend(ROB), RETIRE_WIDTH, [](const auto& x) { return x.executed == COMPLETED; });
  if constexpr (champsim::debug_print) {
    std::for_each(retire_begin, retire_end, [](const auto& x) { std::cout << "[ROB] retire_rob instr_id: " << x.instr_id << " is retired" << std::endl; });
  }
  num_retired += std::distance(retire_begin, retire_end);
  ROB.erase(retire_begin, retire_end);

  // Check for deadlock
  if (!std::empty(ROB) && (ROB.front().event_cycle + DEADLOCK_CYCLE) <= current_cycle)
    throw champsim::deadlock{cpu};
}

void CacheBus::return_data(const PACKET& packet) { PROCESSED.push_back(packet); }

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

  if (!std::empty(ROB)) {
    std::cout << "ROB head";
    std::cout << " instr_id: " << ROB.front().instr_id;
    std::cout << " fetched: " << +ROB.front().fetched;
    std::cout << " scheduled: " << +ROB.front().scheduled;
    std::cout << " executed: " << +ROB.front().executed;
    std::cout << " num_reg_dependent: " << +ROB.front().num_reg_dependent;
    std::cout << " num_mem_ops: " << ROB.front().num_mem_ops() - ROB.front().completed_mem_ops;
    std::cout << " event: " << ROB.front().event_cycle;
    std::cout << std::endl;
  } else {
    std::cout << "ROB empty" << std::endl;
  }

  // print LQ entry
  std::cout << "Load Queue Entry" << std::endl;
  for (auto lq_it = std::begin(LQ); lq_it != std::end(LQ); ++lq_it) {
    if (lq_it->has_value()) {
      std::cout << "[LQ] entry: " << std::distance(std::begin(LQ), lq_it) << " instr_id: " << (*lq_it)->instr_id << " address: " << std::hex
                << (*lq_it)->virtual_address << std::dec << " fetched_issued: " << std::boolalpha << (*lq_it)->fetch_issued << std::noboolalpha
                << " event_cycle: " << (*lq_it)->event_cycle;
      if ((*lq_it)->producer_id != std::numeric_limits<uint64_t>::max())
        std::cout << " waits on " << (*lq_it)->producer_id;
      std::cout << std::endl;
    }
  }

  // print SQ entry
  std::cout << std::endl << "Store Queue Entry" << std::endl;
  for (auto sq_it = std::begin(SQ); sq_it != std::end(SQ); ++sq_it) {
    std::cout << "[SQ] entry: " << std::distance(std::begin(SQ), sq_it) << " instr_id: " << sq_it->instr_id << " address: " << std::hex
              << sq_it->virtual_address << std::dec << " fetched: " << std::boolalpha << sq_it->fetch_issued << std::noboolalpha
              << " event_cycle: " << sq_it->event_cycle << " LQ waiting: ";
    for (std::optional<LSQ_ENTRY>& lq_entry : sq_it->lq_depend_on_me)
      std::cout << lq_entry->instr_id << " ";
    std::cout << std::endl;
  }
}

void LSQ_ENTRY::finish(std::deque<ooo_model_instr>::iterator begin, std::deque<ooo_model_instr>::iterator end) const
{
  auto rob_entry = std::partition_point(begin, end, [id = this->instr_id](auto x) { return x.instr_id < id; });
  assert(rob_entry != end);
  assert(rob_entry->instr_id == this->instr_id);

  ++rob_entry->completed_mem_ops;
  assert(rob_entry->completed_mem_ops <= rob_entry->num_mem_ops());

  if constexpr (champsim::debug_print) {
    std::cout << "[LSQ] " << __func__ << " instr_id: " << instr_id << std::hex;
    std::cout << " full_address: " << virtual_address << std::dec << " remain_mem_ops: " << rob_entry->num_mem_ops() - rob_entry->completed_mem_ops;
    std::cout << " event_cycle: " << event_cycle << std::endl;
  }
}

bool CacheBus::issue_read(PACKET data_packet)
{
  data_packet.address = data_packet.v_address;
  data_packet.cpu = cpu;
  data_packet.type = LOAD;
  data_packet.to_return = {this};

  return lower_level->add_rq(data_packet);
}

bool CacheBus::issue_write(PACKET data_packet)
{
  data_packet.address = data_packet.v_address;
  data_packet.cpu = cpu;
  data_packet.type = WRITE;

  return lower_level->add_wq(data_packet);
}
