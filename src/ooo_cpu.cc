#include "ooo_cpu.h"

#include <algorithm>
#include <vector>

#include "cache.h"
#include "champsim.h"
#include "instruction.h"

#define DEADLOCK_CYCLE 1000000

extern uint8_t warmup_complete[NUM_CPUS];

void O3_CPU::operate()
{
  instrs_to_read_this_cycle = std::min((std::size_t)FETCH_WIDTH, IFETCH_BUFFER.size() - IFETCH_BUFFER.occupancy());

  retire_rob();                    // retire
  complete_inflight_instruction(); // finalize execution
  execute_instruction();           // execute instructions
  schedule_instruction();          // schedule instructions
  handle_memory_return();          // finalize memory transactions
  operate_lsq();                   // execute memory transactions

  dispatch_instruction();        // dispatch
  decode_instruction();          // decode
  promote_to_decode();
  fetch_instruction(); // fetch
  translate_fetch();
  check_dib();

  DECODE_BUFFER.operate();
}

void O3_CPU::initialize_core()
{
  // BRANCH PREDICTOR & BTB
  impl_branch_predictor_initialize();
  impl_btb_initialize();
}

void O3_CPU::init_instruction(ooo_model_instr arch_instr)
{
    instrs_to_read_this_cycle--;

    arch_instr.instr_id = instr_unique_id;

    bool writes_sp = std::count(std::begin(arch_instr.destination_registers), std::end(arch_instr.destination_registers), REG_STACK_POINTER);
    bool writes_ip = std::count(std::begin(arch_instr.destination_registers), std::end(arch_instr.destination_registers), REG_INSTRUCTION_POINTER);
    bool reads_sp = std::count(std::begin(arch_instr.source_registers), std::end(arch_instr.source_registers), REG_STACK_POINTER);
    bool reads_flags = std::count(std::begin(arch_instr.source_registers), std::end(arch_instr.source_registers), REG_FLAGS);
    bool reads_ip = std::count(std::begin(arch_instr.source_registers), std::end(arch_instr.source_registers), REG_INSTRUCTION_POINTER);
    bool reads_other = std::count_if(std::begin(arch_instr.source_registers), std::end(arch_instr.source_registers), [](uint8_t r){ return r != REG_STACK_POINTER && r != REG_FLAGS && r != REG_INSTRUCTION_POINTER; });

    arch_instr.num_mem_ops = std::size(arch_instr.destination_memory) + std::size(arch_instr.source_memory);

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
  }

  total_branch_types[arch_instr.branch_type]++;

  if ((arch_instr.is_branch != 1) || (arch_instr.branch_taken != 1)) {
    // clear the branch target for this instruction
    arch_instr.branch_target = 0;
  }

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
             auto nonsp_end = std::remove(std::begin(arch_instr.destination_registers), std::end(arch_instr.destination_registers), REG_STACK_POINTER);
             arch_instr.destination_registers.erase(nonsp_end, std::end(arch_instr.destination_registers));
    }
  }

  // add this instruction to the IFETCH_BUFFER

  // handle branch prediction
  if (arch_instr.is_branch) {

    DP(if (warmup_complete[cpu]) {
      cout << "[BRANCH] instr_id: " << instr_unique_id << " ip: " << hex << arch_instr.ip << dec << " taken: " << +arch_instr.branch_taken << endl;
    });

    num_branch++;

    std::pair<uint64_t, uint8_t> btb_result = impl_btb_prediction(arch_instr.ip, arch_instr.branch_type);
    uint64_t predicted_branch_target = btb_result.first;
    uint8_t always_taken = btb_result.second;
    uint8_t branch_prediction = impl_predict_branch(arch_instr.ip, predicted_branch_target, always_taken, arch_instr.branch_type);
    if ((branch_prediction == 0) && (always_taken == 0)) {
      predicted_branch_target = 0;
    }

    // call code prefetcher every time the branch predictor is used
    static_cast<CACHE*>(L1I_bus.lower_level)->impl_prefetcher_branch_operate(arch_instr.ip, arch_instr.branch_type, predicted_branch_target);

    if (predicted_branch_target != arch_instr.branch_target) {
      branch_mispredictions++;
      total_rob_occupancy_at_branch_mispredict += std::size(ROB);
      branch_type_misses[arch_instr.branch_type]++;
      if (warmup_complete[cpu]) {
        fetch_stall = 1;
        instrs_to_read_this_cycle = 0;
        arch_instr.branch_mispredicted = 1;
      }
    } else {
      // if correctly predicted taken, then we can't fetch anymore instructions
      // this cycle
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
    if(!warmup_complete[cpu])
      {
          arch_instr.source_registers.clear();
          arch_instr.destination_registers.clear();
      }

  // Add to IFETCH_BUFFER
  IFETCH_BUFFER.push_back(arch_instr);

  instr_unique_id++;
}

void O3_CPU::check_dib()
{
  // scan through IFETCH_BUFFER to find instructions that hit in the decoded
  // instruction buffer
  auto end = std::min(IFETCH_BUFFER.end(), std::next(IFETCH_BUFFER.begin(), FETCH_WIDTH));
  for (auto it = IFETCH_BUFFER.begin(); it != end; ++it)
    do_check_dib(*it);
}

void O3_CPU::do_check_dib(ooo_model_instr& instr)
{
  // Check DIB to see if we recently fetched this line
  auto dib_set_begin = std::next(DIB.begin(), ((instr.ip >> lg2(dib_window)) % dib_set) * dib_way);
  auto dib_set_end = std::next(dib_set_begin, dib_way);
  auto way = std::find_if(dib_set_begin, dib_set_end, eq_addr<dib_t::value_type>(instr.ip, lg2(dib_window)));

  if (way != dib_set_end) {
    // The cache line is in the L0, so we can mark this as complete
    instr.translated = COMPLETED;
    instr.fetched = COMPLETED;

    // Also mark it as decoded
    instr.decoded = COMPLETED;

    // It can be acted on immediately
    instr.event_cycle = current_cycle;

    // Update LRU
    std::for_each(dib_set_begin, dib_set_end, lru_updater<dib_entry_t>(way));
  }
}

void O3_CPU::translate_fetch()
{
  if (IFETCH_BUFFER.empty())
    return;

  // scan through IFETCH_BUFFER to find instructions that need to be translated
  auto itlb_req_begin = std::find_if(IFETCH_BUFFER.begin(), IFETCH_BUFFER.end(), [](const ooo_model_instr& x) { return !x.translated; });
  uint64_t find_addr = itlb_req_begin->ip;
  auto itlb_req_end = std::find_if(itlb_req_begin, IFETCH_BUFFER.end(),
                                   [find_addr](const ooo_model_instr& x) { return (find_addr >> LOG2_PAGE_SIZE) != (x.ip >> LOG2_PAGE_SIZE); });
  if (itlb_req_end != IFETCH_BUFFER.end() || itlb_req_begin == IFETCH_BUFFER.begin()) {
    do_translate_fetch(itlb_req_begin, itlb_req_end);
  }
}

void O3_CPU::do_translate_fetch(champsim::circular_buffer<ooo_model_instr>::iterator begin, champsim::circular_buffer<ooo_model_instr>::iterator end)
{
  // begin process of fetching this instruction by sending it to the ITLB
  // add it to the ITLB's read queue
  PACKET trace_packet;
  trace_packet.address = begin->ip;
  trace_packet.v_address = begin->ip;
  trace_packet.instr_id = begin->instr_id;
  trace_packet.ip = begin->ip;
  trace_packet.type = LOAD;
  trace_packet.instr_depend_on_me = {begin, end};

  int rq_index = ITLB_bus.issue_read(trace_packet);
  if (rq_index != -2) {
    // successfully sent to the ITLB, so mark all instructions in the
    // IFETCH_BUFFER that match this ip as translated INFLIGHT
    for (ooo_model_instr& dep : trace_packet.instr_depend_on_me) {
      dep.translated = INFLIGHT;
    }
  }
}

void O3_CPU::fetch_instruction()
{
  // if we had a branch mispredict, turn fetching back on after the branch
  // mispredict penalty
  if ((fetch_stall == 1) && (current_cycle >= fetch_resume_cycle) && (fetch_resume_cycle != 0)) {
    fetch_stall = 0;
    fetch_resume_cycle = 0;
  }

  if (IFETCH_BUFFER.empty())
    return;

  // fetch cache lines that were part of a translated page but not the cache
  // line that initiated the translation
  auto l1i_req_begin =
      std::find_if(IFETCH_BUFFER.begin(), IFETCH_BUFFER.end(), [](const ooo_model_instr& x) { return x.translated == COMPLETED && !x.fetched; });
  uint64_t find_addr = l1i_req_begin->instruction_pa;
  auto l1i_req_end = std::find_if(l1i_req_begin, IFETCH_BUFFER.end(),
                                  [find_addr](const ooo_model_instr& x) { return (find_addr >> LOG2_BLOCK_SIZE) != (x.instruction_pa >> LOG2_BLOCK_SIZE); });
  if (l1i_req_end != IFETCH_BUFFER.end() || l1i_req_begin == IFETCH_BUFFER.begin()) {

    do_fetch_instruction(l1i_req_begin, l1i_req_end);
  }
}

void O3_CPU::do_fetch_instruction(champsim::circular_buffer<ooo_model_instr>::iterator begin, champsim::circular_buffer<ooo_model_instr>::iterator end)
{
  // add it to the L1-I's read queue
  PACKET fetch_packet;
  fetch_packet.address = begin->instruction_pa;
  fetch_packet.data = begin->instruction_pa;
  fetch_packet.v_address = begin->ip;
  fetch_packet.instr_id = begin->instr_id;
  fetch_packet.ip = begin->ip;
  fetch_packet.type = LOAD;
  fetch_packet.instr_depend_on_me = {begin, end};

  int rq_index = L1I_bus.issue_read(fetch_packet);
  if (rq_index != -2) {
    // mark all instructions from this cache line as having been fetched
    for (ooo_model_instr& dep : fetch_packet.instr_depend_on_me) {
      dep.fetched = INFLIGHT;
    }
  }
}

void O3_CPU::promote_to_decode()
{
  unsigned available_fetch_bandwidth = FETCH_WIDTH;
  while (available_fetch_bandwidth > 0 && !IFETCH_BUFFER.empty() && !DECODE_BUFFER.full() && IFETCH_BUFFER.front().translated == COMPLETED
         && IFETCH_BUFFER.front().fetched == COMPLETED) {
    if (!warmup_complete[cpu] || IFETCH_BUFFER.front().decoded)
      DECODE_BUFFER.push_back_ready(IFETCH_BUFFER.front());
    else
      DECODE_BUFFER.push_back(IFETCH_BUFFER.front());

    IFETCH_BUFFER.pop_front();

    available_fetch_bandwidth--;
  }

  // check for deadlock
  if (!std::empty(IFETCH_BUFFER) && (IFETCH_BUFFER.front().event_cycle + DEADLOCK_CYCLE) <= current_cycle)
    throw champsim::deadlock{cpu};
}

void O3_CPU::decode_instruction()
{
    std::size_t available_decode_bandwidth = DECODE_WIDTH;

    // Send decoded instructions to dispatch
    while (available_decode_bandwidth > 0 && DECODE_BUFFER.has_ready() && !DISPATCH_BUFFER.full()) {
        ooo_model_instr &db_entry = DECODE_BUFFER.front();
        do_dib_update(db_entry);

	// Resume fetch 
	if (db_entry.branch_mispredicted)
	  {
	    // These branches detect the misprediction at decode
	    if ((db_entry.branch_type == BRANCH_DIRECT_JUMP) || (db_entry.branch_type == BRANCH_DIRECT_CALL))
	      {
		// clear the branch_mispredicted bit so we don't attempt to resume fetch again at execute
		db_entry.branch_mispredicted = 0;
		// pay misprediction penalty
		fetch_resume_cycle = current_cycle + BRANCH_MISPREDICT_PENALTY;
	      }
	  }
	
        // Add to dispatch
        db_entry.event_cycle = current_cycle + (warmup_complete[cpu] ? DISPATCH_LATENCY : 0);
        DISPATCH_BUFFER.push_back(std::move(db_entry));
        DECODE_BUFFER.pop_front();

	available_decode_bandwidth--;
  }

  // check for deadlock
  if (!std::empty(DECODE_BUFFER) && (DECODE_BUFFER.front().event_cycle + DEADLOCK_CYCLE) <= current_cycle)
    throw champsim::deadlock{cpu};
}

void O3_CPU::do_dib_update(const ooo_model_instr& instr)
{
  // Search DIB to see if we need to add this instruction
  auto dib_set_begin = std::next(DIB.begin(), ((instr.ip >> lg2(dib_window)) % dib_set) * dib_way);
  auto dib_set_end = std::next(dib_set_begin, dib_way);
  auto way = std::find_if(dib_set_begin, dib_set_end, eq_addr<dib_t::value_type>(instr.ip, lg2(dib_window)));

  // If we did not find the entry in the DIB, find a victim
  if (way == dib_set_end) {
    way = std::max_element(dib_set_begin, dib_set_end, lru_comparator<dib_entry_t>());
    assert(way != dib_set_end);

    // update way
    way->valid = true;
    way->address = instr.ip;
  }

  std::for_each(dib_set_begin, dib_set_end, lru_updater<dib_entry_t>(way));
}

void O3_CPU::dispatch_instruction()
{
  std::size_t available_dispatch_bandwidth = DISPATCH_WIDTH;

  // dispatch DISPATCH_WIDTH instructions into the ROB
  while (available_dispatch_bandwidth > 0 && !std::empty(DISPATCH_BUFFER) && DISPATCH_BUFFER.front().event_cycle < current_cycle && std::size(ROB) != ROB_SIZE
          && ((std::size_t) std::count_if(std::begin(LQ), std::end(LQ), std::not_fn(is_valid<decltype(LQ)::value_type>{})) >= std::size(DISPATCH_BUFFER.front().source_memory))
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
  std::size_t search_bw = SCHEDULER_SIZE;
  for (auto rob_it = std::begin(ROB); rob_it != std::end(ROB) && search_bw > 0; ++rob_it) {
    if (rob_it->scheduled == 0)
      do_scheduling(*rob_it);

    if (rob_it->executed == 0)
      --search_bw;
  }
}

void O3_CPU::do_scheduling(ooo_model_instr &instr)
{
    // Mark register dependencies
    for (auto src_reg : instr.source_registers) {
        if (!std::empty(reg_producers[src_reg])) {
            ooo_model_instr &prior = reg_producers[src_reg].back();
            if (prior.registers_instrs_depend_on_me.empty() || prior.registers_instrs_depend_on_me.back().get().instr_id != instr.instr_id) {
                prior.registers_instrs_depend_on_me.push_back(instr);
                instr.num_reg_dependent++;
            }
        }
    }

    for (auto dreg : instr.destination_registers)
    {
        auto begin = std::begin(reg_producers[dreg]);
        auto end   = std::end(reg_producers[dreg]);
        auto ins   = std::lower_bound(begin, end, instr, [](const ooo_model_instr &lhs, const ooo_model_instr &rhs){ return lhs.instr_id < rhs.instr_id; });
        reg_producers[dreg].insert(ins, std::ref(instr));
    }

    instr.scheduled = COMPLETED;
    instr.event_cycle = current_cycle + (warmup_complete[cpu] ? SCHEDULING_LATENCY : 0);
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

void O3_CPU::do_execution(ooo_model_instr &rob_entry)
{
  rob_entry.executed = INFLIGHT;
  rob_entry.event_cycle = current_cycle + (warmup_complete[cpu] ? EXEC_LATENCY : 0);

  // Mark LQ entries as ready to translate
  for (auto &lq_entry : LQ)
      if (lq_entry.has_value() && lq_entry->instr_id == rob_entry.instr_id)
          lq_entry->event_cycle = current_cycle + (warmup_complete[cpu] ? EXEC_LATENCY : 0);

  // Mark SQ entries as ready to translate
  for (auto &sq_entry : SQ)
      if (sq_entry.instr_id == rob_entry.instr_id)
          sq_entry.event_cycle = current_cycle + (warmup_complete[cpu] ? EXEC_LATENCY : 0);

    DP (if (warmup_complete[cpu]) {
            std::cout << "[ROB] " << __func__ << " instr_id: " << rob_entry.instr_id << " event_cycle: " << rob_entry.event_cycle << std::endl;});
}

void O3_CPU::do_memory_scheduling(ooo_model_instr &instr)
{
    // load
    for (auto& smem : instr.source_memory) {
        auto q_entry = std::find_if_not(std::begin(LQ), std::end(LQ), is_valid<decltype(LQ)::value_type>{});
        assert(q_entry != std::end(LQ));
        q_entry->emplace(LSQ_ENTRY{instr.instr_id, smem, instr.ip, current_cycle + SCHEDULING_LATENCY, std::ref(instr), {instr.asid[0], instr.asid[1]}}); // add it to the load queue

        // Check for forwarding
        auto sq_it = std::max_element(std::begin(SQ), std::end(SQ), [smem](const auto &lhs, const auto &rhs) {
                return lhs.virtual_address != smem || (rhs.virtual_address == smem && lhs.instr_id < rhs.instr_id);
                });
        if (sq_it->virtual_address == smem) {
            if (sq_it->fetch_issued) { // Store already executed
                q_entry->reset();
                instr.num_mem_ops--;

                DP (if (warmup_complete[cpu]) {
                std::cout << "[DISPATCH] " << __func__ << " instr_id: " << instr.instr_id << " forwards from " << sq_it->instr_id << std::endl; })
            } else {
                // this load cannot be executed until the prior store gets executed
                sq_it->lq_depend_on_me.push_back(*q_entry);
                (*q_entry)->producer_id = sq_it->instr_id;

                DP (if (warmup_complete[cpu]) {
                std::cout << "[DISPATCH] " << __func__ << " instr_id: " << instr.instr_id << " waits on " << sq_it->instr_id << std::endl; })
            }
        }
    }

    // store
    for (auto& dmem : instr.destination_memory)
        SQ.push_back({instr.instr_id, dmem, instr.ip, current_cycle + SCHEDULING_LATENCY, std::ref(instr), {instr.asid[0], instr.asid[1]}}); // add it to the store queue

    DP (if (warmup_complete[cpu]) {
    std::cout << "[DISPATCH] " << __func__ << " instr_id: " << instr.instr_id << " loads: " << std::size(instr.source_memory) << " stores: " << std::size(instr.destination_memory) << std::endl; });
}

void O3_CPU::operate_lsq()
{
    auto store_bw = SQ_WIDTH;

    for (auto &sq_entry : SQ) {
        if (store_bw > 0 && sq_entry.physical_address == 0 && !sq_entry.translate_issued && sq_entry.event_cycle < current_cycle) {
            auto result = do_translate_store(sq_entry);
            if (result != -2) {
                --store_bw;
                sq_entry.translate_issued = true;
            }
        }
    }

    for (auto &sq_entry : SQ) {
        if (store_bw > 0 && sq_entry.physical_address != 0 && !sq_entry.fetch_issued && sq_entry.event_cycle < current_cycle) {
            do_finish_store(sq_entry);
            --store_bw;
            sq_entry.fetch_issued = true;
            sq_entry.event_cycle = current_cycle;
        }
    }

    for (; store_bw > 0 && !std::empty(SQ) && SQ.front().instr_id < ROB.front().instr_id && SQ.front().event_cycle < current_cycle; --store_bw) {
        auto result = do_complete_store(SQ.front());
        if (result != -2)
            SQ.pop_front(); // std::deque::erase() requires MoveAssignable :(
        else
            break;
    }

    auto load_bw = LQ_WIDTH;

    for (auto &lq_entry : LQ) {
        if (load_bw > 0 && lq_entry.has_value() && lq_entry->producer_id == std::numeric_limits<uint64_t>::max() && lq_entry->physical_address == 0 && !lq_entry->translate_issued && lq_entry->event_cycle < current_cycle) {
            auto result = do_translate_load(*lq_entry);
            if (result != -2) {
                --load_bw;
                lq_entry->translate_issued = true;
            }
        }
    }

    for (auto &lq_entry : LQ) {
        if (load_bw > 0 && lq_entry.has_value() && lq_entry->physical_address != 0 && !lq_entry->fetch_issued && lq_entry->event_cycle < current_cycle) {
            auto result = execute_load(*lq_entry);
            if (result != -2) {
                --load_bw;
                lq_entry->fetch_issued = true;
            }
        }
    }
}

int O3_CPU::do_translate_store(const LSQ_ENTRY& sq_entry)
{
    PACKET data_packet;
    data_packet.address = sq_entry.virtual_address;
    data_packet.v_address = sq_entry.virtual_address;
    data_packet.instr_id = sq_entry.instr_id;
    data_packet.ip = sq_entry.ip;
    data_packet.type = RFO;

    DP (if (warmup_complete[cpu]) {
            std::cout << "[SQ] " << __func__ << " instr_id: " << sq_entry.instr_id << std::endl; })

    return DTLB_bus.issue_read(data_packet);
}

void O3_CPU::do_finish_store(LSQ_ENTRY& sq_entry)
{
  sq_entry.rob_entry.num_mem_ops--;
  sq_entry.rob_entry.event_cycle = current_cycle;
  assert(sq_entry.rob_entry.num_mem_ops >= 0);

  DP (if (warmup_complete[cpu]) {
        std::cout << "[SQ] " << __func__ << " instr_id: " << sq_entry.instr_id << std::hex;
        std::cout << " full_address: " << sq_entry.physical_address << std::dec << " remain_mem_ops: " << sq_entry.rob_entry.num_mem_ops;
        std::cout << " event_cycle: " << sq_entry.event_cycle << std::endl; });

  // check if this store has dependent loads
  for (std::optional<LSQ_ENTRY> &dependent : sq_entry.lq_depend_on_me) {
      // update corresponding LQ entry
      dependent->rob_entry.num_mem_ops--;
      dependent->rob_entry.event_cycle = current_cycle;

      assert(dependent->producer_id == sq_entry.instr_id);
      assert(dependent->rob_entry.num_mem_ops >= 0);

      dependent.reset();
  }
}

int O3_CPU::do_complete_store(const LSQ_ENTRY& sq_entry)
{
    PACKET data_packet;
    data_packet.address = sq_entry.physical_address;
    data_packet.v_address = sq_entry.virtual_address;
    data_packet.instr_id = sq_entry.instr_id;
    data_packet.ip = sq_entry.ip;
    data_packet.type = RFO;

    DP (if (warmup_complete[cpu]) {
            std::cout << "[SQ] " << __func__ << " instr_id: " << sq_entry.instr_id << std::endl; })

    return L1D_bus.issue_write(data_packet);
}

int O3_CPU::do_translate_load(const LSQ_ENTRY& lq_entry)
{
    PACKET data_packet;
    data_packet.address = lq_entry.virtual_address;
    data_packet.v_address = lq_entry.virtual_address;
    data_packet.instr_id = lq_entry.instr_id;
    data_packet.ip = lq_entry.ip;
    data_packet.type = LOAD;

    DP (if (warmup_complete[cpu]) {
            std::cout << "[LQ] " << __func__ << " instr_id: " << lq_entry.instr_id << std::endl; })

    return DTLB_bus.issue_read(data_packet);
}

int O3_CPU::execute_load(const LSQ_ENTRY& lq_entry)
{
    PACKET data_packet;
    data_packet.address = lq_entry.physical_address;
    data_packet.v_address = lq_entry.virtual_address;
    data_packet.instr_id = lq_entry.instr_id;
    data_packet.ip = lq_entry.ip;
    data_packet.type = LOAD;

    DP (if (warmup_complete[cpu]) {
            std::cout << "[LQ] " << __func__ << " instr_id: " << lq_entry.instr_id << std::endl; })

    return L1D_bus.issue_read(data_packet);
}

void O3_CPU::do_complete_execution(ooo_model_instr &instr)
{
    for (auto dreg : instr.destination_registers)
    {
        auto begin = std::begin(reg_producers[dreg]);
        auto end   = std::end(reg_producers[dreg]);
        auto elem = std::find_if(begin, end, [id=instr.instr_id](ooo_model_instr &x){ return x.instr_id == id; });
        assert(elem != end);
        reg_producers[dreg].erase(elem);
    }

    instr.executed = COMPLETED;

    for (ooo_model_instr &dependent : instr.registers_instrs_depend_on_me)
    {
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
    std::size_t complete_bw = EXEC_WIDTH;
    for (auto rob_it = std::begin(ROB); rob_it != std::end(ROB) && complete_bw > 0; ++rob_it) {
        if ((rob_it->executed == INFLIGHT) && (rob_it->event_cycle <= current_cycle) && rob_it->num_mem_ops == 0) {
            do_complete_execution(*rob_it);
            --complete_bw;
        }
    }
}

void O3_CPU::handle_memory_return()
{
  // Instruction Memory

  int available_fetch_bandwidth = FETCH_WIDTH;
  int to_read = static_cast<CACHE*>(ITLB_bus.lower_level)->MAX_READ;

  while (available_fetch_bandwidth > 0 && to_read > 0 && !ITLB_bus.PROCESSED.empty()) {
    PACKET& itlb_entry = ITLB_bus.PROCESSED.front();

    // mark the appropriate instructions in the IFETCH_BUFFER as translated and
    // ready to fetch
    while (available_fetch_bandwidth > 0 && !itlb_entry.instr_depend_on_me.empty()) {
      ooo_model_instr& fetched = itlb_entry.instr_depend_on_me.front();
      if ((fetched.ip >> LOG2_PAGE_SIZE) == (itlb_entry.address >> LOG2_PAGE_SIZE) && fetched.translated != 0) {
          fetched.translated = COMPLETED;
          fetched.instruction_pa = splice_bits(itlb_entry.data, fetched.ip, LOG2_PAGE_SIZE);

        available_fetch_bandwidth--;
      }

      itlb_entry.instr_depend_on_me.erase(std::begin(itlb_entry.instr_depend_on_me));
    }

    // remove this entry if we have serviced all of its instructions
    if (itlb_entry.instr_depend_on_me.empty())
      ITLB_bus.PROCESSED.pop_front();

    --to_read;
  }

  available_fetch_bandwidth = FETCH_WIDTH;
  to_read = static_cast<CACHE*>(L1I_bus.lower_level)->MAX_READ;

  while (available_fetch_bandwidth > 0 && to_read > 0 && !L1I_bus.PROCESSED.empty()) {
    PACKET& l1i_entry = L1I_bus.PROCESSED.front();

    // this is the L1I cache, so instructions are now fully fetched, so mark
    // them as such
    while (available_fetch_bandwidth > 0 && !l1i_entry.instr_depend_on_me.empty()) {
      ooo_model_instr& fetched = l1i_entry.instr_depend_on_me.front();
      if ((fetched.instruction_pa >> LOG2_BLOCK_SIZE) == (l1i_entry.address >> LOG2_BLOCK_SIZE) && fetched.fetched != 0 && fetched.translated == COMPLETED) {
        fetched.fetched = COMPLETED;
        available_fetch_bandwidth--;
      }

      l1i_entry.instr_depend_on_me.erase(std::begin(l1i_entry.instr_depend_on_me));
    }

    // remove this entry if we have serviced all of its instructions
    if (l1i_entry.instr_depend_on_me.empty())
      L1I_bus.PROCESSED.pop_front();
    --to_read;
  }

  // Data Memory
  to_read = static_cast<CACHE*>(DTLB_bus.lower_level)->MAX_READ;

  while (to_read > 0 && !DTLB_bus.PROCESSED.empty())
	{ // DTLB
	  PACKET &dtlb_entry = DTLB_bus.PROCESSED.front();

      for (auto &sq_entry : SQ)
	    {
            if (sq_entry.translate_issued && sq_entry.physical_address == 0 && sq_entry.virtual_address >> LOG2_PAGE_SIZE == dtlb_entry.address >> LOG2_PAGE_SIZE)
            {
                sq_entry.physical_address = splice_bits(dtlb_entry.data, sq_entry.virtual_address, LOG2_PAGE_SIZE); // translated address
                sq_entry.event_cycle = current_cycle;

              DP (if (warmup_complete[cpu]) {
                  std::cout << "[DTLB_SQ] " << __func__ << " instr_id: " << sq_entry.instr_id << std::hex;
                  std::cout << " full_address: " << sq_entry.physical_address << std::dec << " remain_mem_ops: " << sq_entry.rob_entry.num_mem_ops;
                  std::cout << " event_cycle: " << sq_entry.event_cycle << std::endl; });
            }
	    }

      for (auto &lq_entry : LQ)
	    {
            if (lq_entry.has_value() && lq_entry->translate_issued && lq_entry->physical_address == 0 && lq_entry->virtual_address >> LOG2_PAGE_SIZE == dtlb_entry.address >> LOG2_PAGE_SIZE)
            {
                lq_entry->physical_address = splice_bits(dtlb_entry.data, lq_entry->virtual_address, LOG2_PAGE_SIZE); // translated address
                lq_entry->event_cycle = current_cycle;

              DP (if (warmup_complete[cpu]) {
                  std::cout << "[DTLB_LQ] " << __func__ << " instr_id: " << lq_entry->instr_id << std::hex;
                  std::cout << " full_address: " << lq_entry->physical_address << std::dec << " remain_mem_ops: " << lq_entry->rob_entry.num_mem_ops;
                  std::cout << " event_cycle: " << lq_entry->event_cycle << std::endl; });
            }
	    }

    // remove this entry
    DTLB_bus.PROCESSED.pop_front();
    --to_read;
  }

  to_read = static_cast<CACHE*>(L1D_bus.lower_level)->MAX_READ;
  while (to_read > 0 && !L1D_bus.PROCESSED.empty())
	{ // L1D
	  PACKET &l1d_entry = L1D_bus.PROCESSED.front();

      for (auto &lq_entry : LQ)
      {
          if (lq_entry.has_value() && lq_entry->fetch_issued && lq_entry->physical_address >> LOG2_BLOCK_SIZE == l1d_entry.address >> LOG2_BLOCK_SIZE)
          {
              lq_entry->rob_entry.num_mem_ops--;
              lq_entry->rob_entry.event_cycle = current_cycle;
              lq_entry.reset();

              DP (if (warmup_complete[cpu]) {
                  std::cout << "[L1D_LQ] " << __func__ << " instr_id: " << lq_entry->instr_id << std::hex;
                  std::cout << " full_address: " << lq_entry->physical_address << std::dec << " remain_mem_ops: " << lq_entry->rob_entry.num_mem_ops;
                  std::cout << " event_cycle: " << lq_entry->event_cycle << std::endl; });
          }
      }

	  // remove this entry
	  L1D_bus.PROCESSED.pop_front();
      --to_read;;
    }
}

void O3_CPU::retire_rob()
{
    unsigned retire_bandwidth = RETIRE_WIDTH;

    while (retire_bandwidth > 0 && !ROB.empty() && (ROB.front().executed == COMPLETED))
    {
    // release ROB entry
    DP(if (warmup_complete[cpu]) { cout << "[ROB] " << __func__ << " instr_id: " << ROB.front().instr_id << " is retired" << endl; });

    ROB.pop_front();
    num_retired++;
    retire_bandwidth--;
  }

  // Check for deadlock
  if (!std::empty(ROB) && (ROB.front().event_cycle + DEADLOCK_CYCLE) <= current_cycle)
    throw champsim::deadlock{cpu};
}

void CacheBus::return_data(PACKET* packet)
{
  if (packet->type != PREFETCH) {
    PROCESSED.push_back(*packet);
  }
}

void O3_CPU::print_deadlock()
{
  std::cout << "DEADLOCK! CPU " << cpu << " cycle " << current_cycle << std::endl;

  if (!std::empty(IFETCH_BUFFER)) {
    std::cout << "IFETCH_BUFFER head";
    std::cout << " instr_id: " << IFETCH_BUFFER.front().instr_id;
    std::cout << " translated: " << +IFETCH_BUFFER.front().translated;
    std::cout << " fetched: " << +IFETCH_BUFFER.front().fetched;
    std::cout << " scheduled: " << +IFETCH_BUFFER.front().scheduled;
    std::cout << " executed: " << +IFETCH_BUFFER.front().executed;
    std::cout << " num_reg_dependent: " << +IFETCH_BUFFER.front().num_reg_dependent;
    std::cout << " num_mem_ops: " << +IFETCH_BUFFER.front().num_mem_ops;
    std::cout << " event: " << IFETCH_BUFFER.front().event_cycle;
    std::cout << std::endl;
  } else {
    std::cout << "IFETCH_BUFFER empty" << std::endl;
  }

  if (!std::empty(ROB)) {
    std::cout << "ROB head";
    std::cout << " instr_id: " << ROB.front().instr_id;
    std::cout << " translated: " << +ROB.front().translated;
    std::cout << " fetched: " << +ROB.front().fetched;
    std::cout << " scheduled: " << +ROB.front().scheduled;
    std::cout << " executed: " << +ROB.front().executed;
    std::cout << " num_reg_dependent: " << +ROB.front().num_reg_dependent;
    std::cout << " num_mem_ops: " << +ROB.front().num_mem_ops;
    std::cout << " event: " << ROB.front().event_cycle;
    std::cout << std::endl;
  } else {
    std::cout << "ROB empty" << std::endl;
  }

  // print LQ entry
  std::cout << "Load Queue Entry" << std::endl;
  for (auto lq_it = std::begin(LQ); lq_it != std::end(LQ); ++lq_it) {
    if (lq_it->has_value())
      std::cout << "[LQ] entry: " << std::distance(std::begin(LQ), lq_it)
          << " instr_id: " << (*lq_it)->instr_id
          << " address: " << std::hex << (*lq_it)->physical_address << std::dec
          << " translate_issued: " << std::boolalpha << (*lq_it)->translate_issued
          << " translated: " << ((*lq_it)->physical_address != 0)
          << " fetched: " << (*lq_it)->fetch_issued << std::noboolalpha
          << " event_cycle: " << (*lq_it)->event_cycle
          << std::endl;
  }

  // print SQ entry
  std::cout << std::endl << "Store Queue Entry" << std::endl;
  for (auto sq_it = std::begin(SQ); sq_it != std::end(SQ); ++sq_it) {
      std::cout << "[SQ] entry: " << std::distance(std::begin(SQ), sq_it)
          << " instr_id: " << sq_it->instr_id
          << " address: " << std::hex << sq_it->physical_address << std::dec
          << " translate_issued: " << std::boolalpha << sq_it->translate_issued
          << " translated: " << std::boolalpha << (sq_it->physical_address != 0)
          << " fetched: " << sq_it->fetch_issued << std::noboolalpha
          << " event_cycle: " << sq_it->event_cycle
          << std::endl;
  }
}

int CacheBus::issue_read(PACKET data_packet)
{
    data_packet.fill_level = lower_level->fill_level;
    data_packet.cpu = cpu;
    data_packet.to_return = {this};

    return lower_level->add_rq(&data_packet);
}

int CacheBus::issue_write(PACKET data_packet)
{
    data_packet.fill_level = lower_level->fill_level;
    data_packet.cpu = cpu;

    return lower_level->add_wq(&data_packet);
}

