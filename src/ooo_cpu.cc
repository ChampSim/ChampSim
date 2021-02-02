#include <algorithm>
#include <vector>

#include "ooo_cpu.h"
#include "instruction.h"
#include "set.h"
#include "vmem.h"

#define DEADLOCK_CYCLE 1000000

extern uint8_t warmup_complete[NUM_CPUS];
extern uint8_t knob_cloudsuite;
extern uint8_t MAX_INSTR_DESTINATIONS;

extern VirtualMemory vmem;

void O3_CPU::operate()
{
    operated = true;
    instrs_to_read_this_cycle = std::min((std::size_t)FETCH_WIDTH, IFETCH_BUFFER.size() - IFETCH_BUFFER.occupancy());

    retire_rob(); // retire
    complete_inflight_instruction(); // finalize execution
    execute_instruction(); // execute instructions
    schedule_instruction(); // schedule instructions
    handle_memory_return(); // finalize memory transactions
    operate_lsq(); // execute memory transactions

    // operate caches
    static_cast<CACHE*>(ITLB_bus.lower_level)->operate_writes();
    static_cast<CACHE*>(DTLB_bus.lower_level)->operate_writes();
    static_cast<CACHE*>(static_cast<CACHE*>(DTLB_bus.lower_level)->lower_level)->operate_writes();
    static_cast<CACHE*>(L1I_bus.lower_level)->operate_writes();
    static_cast<CACHE*>(L1D_bus.lower_level)->operate_writes();
    static_cast<CACHE*>(static_cast<CACHE*>(L1D_bus.lower_level)->lower_level)->operate_writes();

    static_cast<CACHE*>(static_cast<CACHE*>(L1D_bus.lower_level)->lower_level)->operate_reads();
    static_cast<CACHE*>(L1D_bus.lower_level)->operate_reads();
    static_cast<CACHE*>(L1I_bus.lower_level)->operate_reads();
    static_cast<CACHE*>(static_cast<CACHE*>(DTLB_bus.lower_level)->lower_level)->operate_reads();
    static_cast<CACHE*>(DTLB_bus.lower_level)->operate_reads();
    static_cast<CACHE*>(ITLB_bus.lower_level)->operate_reads();

    // also handle per-cycle prefetcher operation
    l1i_prefetcher_cycle_operate();

    schedule_memory_instruction(); // schedule memory transactions
    dispatch_instruction(); // dispatch
    decode_instruction(); // decode
    fetch_instruction(); // fetch

    // check for deadlock
    if (ROB.entry[ROB.head].ip && (ROB.entry[ROB.head].event_cycle + DEADLOCK_CYCLE) <= current_cycle)
        print_deadlock(cpu);
}

void O3_CPU::initialize_core()
{

}

void O3_CPU::init_instruction(ooo_model_instr arch_instr)
{
    // actual processors do not work like this but for easier implementation,
    // we read instruction traces and virtually add them in the ROB
    // note that these traces are not yet translated and fetched
    instrs_to_read_this_cycle--;

    // first, read PIN trace

    arch_instr.instr_id = instr_unique_id;

    bool reads_sp = false;
    bool writes_sp = false;
    bool reads_flags = false;
    bool reads_ip = false;
    bool writes_ip = false;
    bool reads_other = false;

    for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++)
    {
        switch(arch_instr.destination_registers[i])
        {
            case 0:
                break;
            case REG_STACK_POINTER:
                writes_sp = true;
                break;
            case REG_INSTRUCTION_POINTER:
                writes_ip = true;
                break;
            default:
                break;
        }

        /*
           if((arch_instr.is_branch) && (arch_instr.destination_registers[i] > 24) && (arch_instr.destination_registers[i] < 28))
           {
           arch_instr.destination_registers[i] = 0;
           }
           */

        if (arch_instr.destination_registers[i])
            arch_instr.num_reg_ops++;
        if (arch_instr.destination_memory[i])
        {
            arch_instr.num_mem_ops++;

            // update STA, this structure is required to execute store instructions properly without deadlock
            if (arch_instr.num_mem_ops > 0)
            {
#ifdef SANITY_CHECK
                assert(STA.size() < ROB.SIZE*NUM_INSTR_DESTINATIONS_SPARC);
#endif
                STA.push(instr_unique_id);
            }
        }
    }

    for (int i=0; i<NUM_INSTR_SOURCES; i++)
    {
        switch(arch_instr.source_registers[i])
        {
            case 0:
                break;
            case REG_STACK_POINTER:
                reads_sp = true;
                break;
            case REG_FLAGS:
                reads_flags = true;
                break;
            case REG_INSTRUCTION_POINTER:
                reads_ip = true;
                break;
            default:
                reads_other = true;
                break;
        }

        /*
           if((!arch_instr.is_branch) && (arch_instr.source_registers[i] > 25) && (arch_instr.source_registers[i] < 28))
           {
           arch_instr.source_registers[i] = 0;
           }
           */

        if (arch_instr.source_registers[i])
            arch_instr.num_reg_ops++;
        if (arch_instr.source_memory[i])
            arch_instr.num_mem_ops++;
    }

    if (arch_instr.num_mem_ops > 0)
        arch_instr.is_memory = 1;

    // determine what kind of branch this is, if any
    if(!reads_sp && !reads_flags && writes_ip && !reads_other)
    {
        // direct jump
        arch_instr.is_branch = 1;
        arch_instr.branch_taken = 1;
        arch_instr.branch_type = BRANCH_DIRECT_JUMP;
    }
    else if(!reads_sp && !reads_flags && writes_ip && reads_other)
    {
        // indirect branch
        arch_instr.is_branch = 1;
        arch_instr.branch_taken = 1;
        arch_instr.branch_type = BRANCH_INDIRECT;
    }
    else if(!reads_sp && reads_ip && !writes_sp && writes_ip && reads_flags && !reads_other)
    {
        // conditional branch
        arch_instr.is_branch = 1;
        arch_instr.branch_taken = arch_instr.branch_taken; // don't change this
        arch_instr.branch_type = BRANCH_CONDITIONAL;
    }
    else if(reads_sp && reads_ip && writes_sp && writes_ip && !reads_flags && !reads_other)
    {
        // direct call
        arch_instr.is_branch = 1;
        arch_instr.branch_taken = 1;
        arch_instr.branch_type = BRANCH_DIRECT_CALL;
    }
    else if(reads_sp && reads_ip && writes_sp && writes_ip && !reads_flags && reads_other)
    {
        // indirect call
        arch_instr.is_branch = 1;
        arch_instr.branch_taken = 1;
        arch_instr.branch_type = BRANCH_INDIRECT_CALL;
    }
    else if(reads_sp && !reads_ip && writes_sp && writes_ip)
    {
        // return
        arch_instr.is_branch = 1;
        arch_instr.branch_taken = 1;
        arch_instr.branch_type = BRANCH_RETURN;
    }
    else if(writes_ip)
    {
        // some other branch type that doesn't fit the above categories
        arch_instr.is_branch = 1;
        arch_instr.branch_taken = arch_instr.branch_taken; // don't change this
        arch_instr.branch_type = BRANCH_OTHER;
    }

    total_branch_types[arch_instr.branch_type]++;

    if((arch_instr.is_branch != 1) || (arch_instr.branch_taken != 1))
      {
	// clear the branch target for this instruction
	arch_instr.branch_target = 0;
      }

    // Stack Pointer Folding
    // The exact, true value of the stack pointer for any given instruction can
    // usually be determined immediately after the instruction is decoded without
    // waiting for the stack pointer's dependency chain to be resolved.
    // We're doing it here because we already have writes_sp and reads_other handy,
    // and in ChampSim it doesn't matter where before execution you do it.
    if(writes_sp)
      {
       // Avoid creating register dependencies on the stack pointer for calls, returns, pushes,
       // and pops, but not for variable-sized changes in the stack pointer position.
       // reads_other indicates that the stack pointer is being changed by a variable amount,
       // which can't be determined before execution.
       if((arch_instr.is_branch != 0) || (arch_instr.num_mem_ops > 0) || (!reads_other))
         {
           for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++)
             {
               if(arch_instr.destination_registers[i] == REG_STACK_POINTER)
                 {
                   arch_instr.destination_registers[i] = 0;
                   arch_instr.num_reg_ops--;
                 }
             }
         }
      }

    // add this instruction to the IFETCH_BUFFER

    // handle branch prediction
    if (arch_instr.is_branch) {

        DP( if (warmup_complete[cpu]) {
                cout << "[BRANCH] instr_id: " << instr_unique_id << " ip: " << hex << arch_instr.ip << dec << " taken: " << +arch_instr.branch_taken << endl; });

        num_branch++;

	std::pair<uint64_t, uint8_t> btb_result = btb_prediction(arch_instr.ip, arch_instr.branch_type);
	uint64_t predicted_branch_target = btb_result.first;
	uint8_t always_taken = btb_result.second;
	uint8_t branch_prediction = predict_branch(arch_instr.ip, predicted_branch_target, always_taken, arch_instr.branch_type);
	if((branch_prediction == 0) && (always_taken == 0))
	  {
	    predicted_branch_target = 0;
	  }

        // call code prefetcher every time the branch predictor is used
        l1i_prefetcher_branch_operate(arch_instr.ip, arch_instr.branch_type, predicted_branch_target);

        if(predicted_branch_target != arch_instr.branch_target)
        {
            branch_mispredictions++;
            total_rob_occupancy_at_branch_mispredict += ROB.occupancy;
	    branch_type_misses[arch_instr.branch_type]++;
            if(warmup_complete[cpu])
            {
                fetch_stall = 1;
                instrs_to_read_this_cycle = 0;
                arch_instr.branch_mispredicted = 1;
            }
        }
        else
        {
            // if correctly predicted taken, then we can't fetch anymore instructions this cycle
            if(arch_instr.branch_taken == 1)
            {
                instrs_to_read_this_cycle = 0;
            }
        }

	update_btb(arch_instr.ip, arch_instr.branch_target, arch_instr.branch_taken, arch_instr.branch_type);
        last_branch_result(arch_instr.ip, arch_instr.branch_target, arch_instr.branch_taken, arch_instr.branch_type);
    }

    arch_instr.event_cycle = current_cycle;

    // fast warmup eliminates register dependencies between instructions
    // branch predictor, cache contents, and prefetchers are still warmed up
    if(!warmup_complete[cpu])
      {
	for (int i=0; i<NUM_INSTR_SOURCES; i++)
	  {
	    arch_instr.source_registers[i] = 0;
	  }
	for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++)
	  {
	    arch_instr.destination_registers[i] = 0;
	  }
	arch_instr.num_reg_ops = 0;
      }

    // Add to IFETCH_BUFFER
    IFETCH_BUFFER.push_back(arch_instr);

    instr_unique_id++;
}

void O3_CPU::fetch_instruction()
{
  // TODO: can we model wrong path execusion?
  // probalby not
  
  // if we had a branch mispredict, turn fetching back on after the branch mispredict penalty
  if((fetch_stall == 1) && (current_cycle >= fetch_resume_cycle) && (fetch_resume_cycle != 0))
    {
      fetch_stall = 0;
      fetch_resume_cycle = 0;
    }

  if (IFETCH_BUFFER.empty())
      return;

  // scan through IFETCH_BUFFER to find instructions that hit in the decoded instruction buffer
  auto end = std::min(IFETCH_BUFFER.end(), std::next(IFETCH_BUFFER.begin(), FETCH_WIDTH));
  for (auto it = IFETCH_BUFFER.begin(); it != end; ++it)
  {
      // Check DIB to see if we recently fetched this line
      auto dib_set_begin = std::next(DIB.begin(), ((it->ip >> lg2(dib_window)) % dib_set) * dib_way);
      auto dib_set_end   = std::next(dib_set_begin, dib_way);
      auto way = std::find_if(dib_set_begin, dib_set_end, eq_addr<dib_t::value_type>(it->ip, lg2(dib_window)));
      if (way != dib_set_end)
      {
          // The cache line is in the L0, so we can mark this as complete
          it->translated = COMPLETED;
          it->fetched = COMPLETED;

          // Also mark it as decoded
          it->decoded = COMPLETED;

          // Update LRU
          unsigned hit_lru = way->lru;
          std::for_each(dib_set_begin, dib_set_end, [hit_lru](dib_entry_t &x){ if (x.lru <= hit_lru) x.lru++; });
          way->lru = 0;
      }
  }

  // scan through IFETCH_BUFFER to find instructions that need to be translated
  auto itlb_req_begin = std::find_if(IFETCH_BUFFER.begin(), IFETCH_BUFFER.end(), [](const ooo_model_instr &x){ return !x.translated; });
  uint64_t find_addr = itlb_req_begin->ip;
  auto itlb_req_end   = std::find_if(itlb_req_begin, IFETCH_BUFFER.end(), [find_addr](const ooo_model_instr &x){ return (find_addr >> LOG2_PAGE_SIZE) != (x.ip >> LOG2_PAGE_SIZE);});
  if (itlb_req_end != IFETCH_BUFFER.end() || itlb_req_begin == IFETCH_BUFFER.begin())
	{
	  // begin process of fetching this instruction by sending it to the ITLB
	  // add it to the ITLB's read queue
	  PACKET trace_packet;
	  trace_packet.fill_level = FILL_L1;
	  trace_packet.cpu = cpu;
          trace_packet.address = itlb_req_begin->ip >> LOG2_PAGE_SIZE;
          trace_packet.full_addr = itlb_req_begin->ip;
	  trace_packet.instr_id = itlb_req_begin->instr_id;
          trace_packet.ip = itlb_req_begin->ip;
	  trace_packet.type = LOAD; 
	  trace_packet.asid[0] = 0;
	  trace_packet.asid[1] = 0;
	  trace_packet.to_return = {&ITLB_bus};
      for (auto dep_it = itlb_req_begin; dep_it != itlb_req_end; ++dep_it)
          trace_packet.instr_depend_on_me.push_back(dep_it);

      int rq_index = ITLB_bus.lower_level->add_rq(&trace_packet);

	  if(rq_index != -2)
	    {
	      // successfully sent to the ITLB, so mark all instructions in the IFETCH_BUFFER that match this ip as translated INFLIGHT
            for (auto dep_it : trace_packet.instr_depend_on_me)
            {
                dep_it->translated = INFLIGHT;
            }
	    }
	}


      // fetch cache lines that were part of a translated page but not the cache line that initiated the translation
    auto l1i_req_begin = std::find_if(IFETCH_BUFFER.begin(), IFETCH_BUFFER.end(),
            [](const ooo_model_instr &x){ return x.translated == COMPLETED && !x.fetched; });
    find_addr = l1i_req_begin->instruction_pa;
    auto l1i_req_end   = std::find_if(l1i_req_begin, IFETCH_BUFFER.end(),
            [find_addr](const ooo_model_instr &x){ return (find_addr >> LOG2_BLOCK_SIZE) != (x.instruction_pa >> LOG2_BLOCK_SIZE);});
    if (l1i_req_end != IFETCH_BUFFER.end() || l1i_req_begin == IFETCH_BUFFER.begin())
	{
	  // add it to the L1-I's read queue
	  PACKET fetch_packet;
	  fetch_packet.fill_level = FILL_L1;
	  fetch_packet.cpu = cpu;
          fetch_packet.address = l1i_req_begin->instruction_pa >> LOG2_BLOCK_SIZE;
          fetch_packet.data = l1i_req_begin->instruction_pa;
          fetch_packet.full_addr = l1i_req_begin->instruction_pa;
          fetch_packet.v_address = l1i_req_begin->ip >> LOG2_PAGE_SIZE;
          fetch_packet.full_v_addr = l1i_req_begin->ip;
	  fetch_packet.instr_id = l1i_req_begin->instr_id;
          fetch_packet.ip = l1i_req_begin->ip;
	  fetch_packet.type = LOAD; 
	  fetch_packet.asid[0] = 0;
	  fetch_packet.asid[1] = 0;
	  fetch_packet.to_return = {&L1I_bus};
      for (auto dep_it = l1i_req_begin; dep_it != l1i_req_end; ++dep_it)
          fetch_packet.instr_depend_on_me.push_back(dep_it);

      int rq_index = L1I_bus.lower_level->add_rq(&fetch_packet);

	  if(rq_index != -2)
	    {
	      // mark all instructions from this cache line as having been fetched
            for (auto dep_it : fetch_packet.instr_depend_on_me)
            {
                dep_it->fetched = INFLIGHT;
            }
	    }
	}

    // send to DECODE stage
    unsigned available_fetch_bandwidth = FETCH_WIDTH;
    while (available_fetch_bandwidth > 0 && !IFETCH_BUFFER.empty() && !DECODE_BUFFER.full() &&
            IFETCH_BUFFER.front().translated == COMPLETED && IFETCH_BUFFER.front().fetched == COMPLETED)
    {
        // ADD to decode buffer
        if (!warmup_complete[cpu] || IFETCH_BUFFER.front().decoded)
            DECODE_BUFFER.push_back_ready(IFETCH_BUFFER.front());
        else
            DECODE_BUFFER.push_back(IFETCH_BUFFER.front());

        IFETCH_BUFFER.pop_front();

	available_fetch_bandwidth--;
    }
}


void O3_CPU::decode_instruction()
{
    if (DECODE_BUFFER.empty())
        return;
    
    std::size_t available_decode_bandwidth = DECODE_WIDTH;

    // Send decoded instructions to dispatch
    while (available_decode_bandwidth > 0 && DECODE_BUFFER.has_ready() && !DISPATCH_BUFFER.full())
    {
        ooo_model_instr &db_entry = DECODE_BUFFER.front();

	// Search DIB to see if we need to add this instruction
      auto dib_set_begin = std::next(DIB.begin(), ((db_entry.ip >> lg2(dib_window)) % dib_set) * dib_way);
      auto dib_set_end   = std::next(dib_set_begin, dib_way);
      auto way = std::find_if(dib_set_begin, dib_set_end, eq_addr<dib_t::value_type>(db_entry.ip, lg2(dib_window)));
	
	// If we did not find the entry in the DIB, find a victim
	if (way == dib_set_end)
	{
	  way = std::max_element(dib_set_begin, dib_set_end, [](dib_entry_t x, dib_entry_t y){ return !y.valid || (x.valid && x.lru < y.lru); }); // invalid ways compare LRU
	  assert(way != dib_set_end);
	}
	
	// update LRU in DIB
	unsigned hit_lru = way->lru;
	std::for_each(dib_set_begin, dib_set_end, [hit_lru](dib_entry_t &x){ if (x.lru <= hit_lru) x.lru++; });
	
        // update way
	way->valid = true;
	way->lru = 0;
	way->address = db_entry.ip;

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
        if (warmup_complete[cpu])
            DISPATCH_BUFFER.push_back(db_entry);
        else
            DISPATCH_BUFFER.push_back_ready(db_entry);
        DECODE_BUFFER.pop_front();

	available_decode_bandwidth--;
    }

    DECODE_BUFFER.operate();
}

void O3_CPU::dispatch_instruction()
{
    if (DISPATCH_BUFFER.empty())
        return;

    std::size_t available_dispatch_bandwidth = DISPATCH_WIDTH;

    // dispatch DISPATCH_WIDTH instructions into the ROB
    while (available_dispatch_bandwidth > 0 && DISPATCH_BUFFER.has_ready() && ROB.occupancy < ROB.SIZE)
    {
        // Add to ROB
        ROB.entry[ROB.tail] = DISPATCH_BUFFER.front();
        ROB.entry[ROB.tail].event_cycle = current_cycle;

        ROB.tail++;
        if (ROB.tail >= ROB.SIZE)
            ROB.tail = 0;
        ROB.occupancy++;

        DISPATCH_BUFFER.pop_front();
	available_dispatch_bandwidth--;
    }

    DISPATCH_BUFFER.operate();
}

int O3_CPU::prefetch_code_line(uint64_t pf_v_addr)
{
    return static_cast<CACHE*>(L1I_bus.lower_level)->va_prefetch_line(pf_v_addr, pf_v_addr, FILL_L1, 0);
}

void O3_CPU::schedule_instruction()
{
    if ((ROB.head == ROB.tail) && ROB.occupancy == 0)
        return;

    num_searched = 0;
    for (uint32_t i=ROB.head, count=0; count<ROB.occupancy; i=(i+1==ROB.SIZE) ? 0 : i+1, count++) {
        if ((ROB.entry[i].fetched != COMPLETED) || (ROB.entry[i].event_cycle > current_cycle) || (num_searched >= SCHEDULER_SIZE))
            return;

        if (ROB.entry[i].scheduled == 0)
            do_scheduling(i);

        if(ROB.entry[i].executed == 0)
            num_searched++;
    }
}

void O3_CPU::do_scheduling(uint32_t rob_index)
{
    ROB.entry[rob_index].reg_ready = 1; // reg_ready will be reset to 0 if there is RAW dependency 

    reg_dependency(rob_index);

    if (ROB.entry[rob_index].is_memory)
        ROB.entry[rob_index].scheduled = INFLIGHT;
    else {
        ROB.entry[rob_index].scheduled = COMPLETED;

        // ADD LATENCY
        ROB.entry[rob_index].event_cycle = current_cycle + (warmup_complete[cpu] ? SCHEDULING_LATENCY : 0);

        if (ROB.entry[rob_index].reg_ready) {

#ifdef SANITY_CHECK
            assert(ready_to_execute.size() <= ROB.SIZE);
#endif
            ready_to_execute.push(rob_index);

            DP (if (warmup_complete[cpu]) {
                    std::cout << "[ready_to_execute] " << __func__ << " instr_id: " << ROB.entry[rob_index].instr_id << " rob_index: " << rob_index << " is added to ready_to_execute" << std::endl; });
        }
    }
}

void O3_CPU::reg_dependency(uint32_t rob_index)
{
    // print out source/destination registers
    DP (if (warmup_complete[cpu]) {
    for (uint32_t i=0; i<NUM_INSTR_SOURCES; i++) {
        if (ROB.entry[rob_index].source_registers[i]) {
            cout << "[ROB] " << __func__ << " instr_id: " << ROB.entry[rob_index].instr_id << " is_memory: " << +ROB.entry[rob_index].is_memory;
            cout << " load  reg_index: " << +ROB.entry[rob_index].source_registers[i] << endl;
        }
    }
    for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
        if (ROB.entry[rob_index].destination_registers[i]) {
            cout << "[ROB] " << __func__ << " instr_id: " << ROB.entry[rob_index].instr_id << " is_memory: " << +ROB.entry[rob_index].is_memory;
            cout << " store reg_index: " << +ROB.entry[rob_index].destination_registers[i] << endl;
        }
    } }); 

    // check RAW dependency
    int prior = rob_index - 1;
    if (prior < 0)
        prior = ROB.SIZE - 1;

    if (rob_index != ROB.head) {
        if ((int)ROB.head <= prior) {
            for (int i=prior; i>=(int)ROB.head; i--) if (ROB.entry[i].executed != COMPLETED) {
		for (uint32_t j=0; j<NUM_INSTR_SOURCES; j++) {
			if (ROB.entry[rob_index].source_registers[j] && (ROB.entry[rob_index].reg_RAW_checked[j] == 0))
				reg_RAW_dependency(i, rob_index, j);
		}
	    }
        } else {
            for (int i=prior; i>=0; i--) if (ROB.entry[i].executed != COMPLETED) {
		for (uint32_t j=0; j<NUM_INSTR_SOURCES; j++) {
			if (ROB.entry[rob_index].source_registers[j] && (ROB.entry[rob_index].reg_RAW_checked[j] == 0))
				reg_RAW_dependency(i, rob_index, j);
		}
	    }
            for (int i=ROB.SIZE-1; i>=(int)ROB.head; i--) if (ROB.entry[i].executed != COMPLETED) {
		for (uint32_t j=0; j<NUM_INSTR_SOURCES; j++) {
			if (ROB.entry[rob_index].source_registers[j] && (ROB.entry[rob_index].reg_RAW_checked[j] == 0))
				reg_RAW_dependency(i, rob_index, j);
		}
	    }
        }
    }
}

void O3_CPU::reg_RAW_dependency(uint32_t prior, uint32_t current, uint32_t source_index)
{
    for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
        if (ROB.entry[prior].destination_registers[i] == 0)
            continue;

        if (ROB.entry[prior].destination_registers[i] == ROB.entry[current].source_registers[source_index]) {

            // we need to mark this dependency in the ROB since the producer might not be added in the store queue yet
            ROB.entry[prior].registers_instrs_depend_on_me.insert (current);   // this load cannot be executed until the prior store gets executed
            ROB.entry[prior].registers_index_depend_on_me[source_index].insert (current);   // this load cannot be executed until the prior store gets executed
            ROB.entry[prior].reg_RAW_producer = 1;

            ROB.entry[current].reg_ready = 0;
            ROB.entry[current].producer_id = ROB.entry[prior].instr_id; 
            ROB.entry[current].num_reg_dependent++;
            ROB.entry[current].reg_RAW_checked[source_index] = 1;

            DP (if(warmup_complete[cpu]) {
            cout << "[ROB] " << __func__ << " instr_id: " << ROB.entry[current].instr_id << " is_memory: " << +ROB.entry[current].is_memory;
            cout << " RAW reg_index: " << +ROB.entry[current].source_registers[source_index];
            cout << " producer_id: " << ROB.entry[prior].instr_id << endl; });

            return;
        }
    }
}

void O3_CPU::execute_instruction()
{
    if ((ROB.head == ROB.tail) && ROB.occupancy == 0)
        return;

    // out-of-order execution for non-memory instructions
    // memory instructions are handled by memory_instruction()
    uint32_t exec_issued = 0, num_iteration = 0;
    
    while (exec_issued < EXEC_WIDTH) {
        if (!ready_to_execute.empty()) {
            uint32_t exec_index = ready_to_execute.front();
            if (ROB.entry[exec_index].event_cycle <= current_cycle) {
                do_execution(exec_index);

                ready_to_execute.pop();
                exec_issued++;
            }
        }
        else {
            break;
        }

        num_iteration++;
        if (num_iteration == (ROB.SIZE-1))
            break;
    }
}

void O3_CPU::do_execution(uint32_t rob_index)
{
    //if (ROB.entry[rob_index].reg_ready && (ROB.entry[rob_index].scheduled == COMPLETED) && (ROB.entry[rob_index].event_cycle <= current_cycle)) {

  //cout << "do_execution() rob_index: " << rob_index << " cycle: " << current_cycle << endl;
  
        ROB.entry[rob_index].executed = INFLIGHT;

        // ADD LATENCY
        ROB.entry[rob_index].event_cycle = current_cycle + (warmup_complete[cpu] ? EXEC_LATENCY : 0);

        inflight_reg_executions++;

        DP (if (warmup_complete[cpu]) {
        cout << "[ROB] " << __func__ << " non-memory instr_id: " << ROB.entry[rob_index].instr_id; 
        cout << " event_cycle: " << ROB.entry[rob_index].event_cycle << endl;});
    //}
}

uint8_t O3_CPU::mem_reg_dependence_resolved(uint32_t rob_index)
{
  return ROB.entry[rob_index].reg_ready;
}

void O3_CPU::schedule_memory_instruction()
{
    if ((ROB.head == ROB.tail) && ROB.occupancy == 0)
        return;

    // execution is out-of-order but we have an in-order scheduling algorithm to detect all RAW dependencies
    num_searched = 0;
    for (uint32_t i=ROB.head, count=0; count<ROB.occupancy; i=(i+1==ROB.SIZE) ? 0 : i+1, count++) {
        if ((ROB.entry[i].fetched != COMPLETED) || (ROB.entry[i].event_cycle > current_cycle) || (num_searched >= SCHEDULER_SIZE))
            break;

        if (ROB.entry[i].is_memory && mem_reg_dependence_resolved(i) && (ROB.entry[i].scheduled == INFLIGHT))
            do_memory_scheduling(i);

        if (ROB.entry[i].executed == 0)
            num_searched++;
    }
}

void O3_CPU::do_memory_scheduling(uint32_t rob_index)
{
    uint32_t not_available = check_and_add_lsq(rob_index);
    if (not_available == 0) {
        ROB.entry[rob_index].scheduled = COMPLETED;
        if (ROB.entry[rob_index].executed == 0) // it could be already set to COMPLETED due to store-to-load forwarding
            ROB.entry[rob_index].executed  = INFLIGHT;

        DP (if (warmup_complete[cpu]) {
        cout << "[ROB] " << __func__ << " instr_id: " << ROB.entry[rob_index].instr_id << " rob_index: " << rob_index;
        cout << " scheduled all num_mem_ops: " << ROB.entry[rob_index].num_mem_ops << endl; });
    }
}

uint32_t O3_CPU::check_and_add_lsq(uint32_t rob_index) 
{
    uint32_t num_mem_ops = 0, num_added = 0;

    // load
    for (uint32_t i=0; i<NUM_INSTR_SOURCES; i++) {
        if (ROB.entry[rob_index].source_memory[i]) {
            num_mem_ops++;
            if (ROB.entry[rob_index].source_added[i])
                num_added++;
            else if (LQ.occupancy < LQ.SIZE) {
                add_load_queue(rob_index, i);
                num_added++;
            }
            else {
                DP(if(warmup_complete[cpu]) {
                cout << "[LQ] " << __func__ << " instr_id: " << ROB.entry[rob_index].instr_id;
                cout << " cannot be added in the load queue occupancy: " << LQ.occupancy << " cycle: " << current_cycle << endl; });
            }
        }
    }

    // store
    for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
        if (ROB.entry[rob_index].destination_memory[i]) {
            num_mem_ops++;
            if (ROB.entry[rob_index].destination_added[i])
                num_added++;
            else if (SQ.occupancy < SQ.SIZE) {
                if (STA.front() == ROB.entry[rob_index].instr_id) {
                    add_store_queue(rob_index, i);
                    num_added++;
                }
                //add_store_queue(rob_index, i);
                //num_added++;
            }
            else {
                DP(if(warmup_complete[cpu]) {
                cout << "[SQ] " << __func__ << " instr_id: " << ROB.entry[rob_index].instr_id;
                cout << " cannot be added in the store queue occupancy: " << SQ.occupancy << " cycle: " << current_cycle << endl; });
            }
        }
    }

    if (num_added == num_mem_ops)
        return 0;

    uint32_t not_available = num_mem_ops - num_added;
    if (not_available > num_mem_ops) {
        cerr << "instr_id: " << ROB.entry[rob_index].instr_id << endl;
        assert(0);
    }

    return not_available;
}

void O3_CPU::add_load_queue(uint32_t rob_index, uint32_t data_index)
{
    // search for an empty slot 
    uint32_t lq_index = LQ.SIZE;
    for (uint32_t i=0; i<LQ.SIZE; i++) {
        if (LQ.entry[i].virtual_address == 0) {
            lq_index = i;
            break;
        }
    }

    // sanity check
    if (lq_index == LQ.SIZE) {
        cerr << "instr_id: " << ROB.entry[rob_index].instr_id << " no empty slot in the load queue!!!" << endl;
        assert(0);
    }

    // add it to the load queue
    ROB.entry[rob_index].lq_index[data_index] = lq_index;
    LQ.entry[lq_index].instr_id = ROB.entry[rob_index].instr_id;
    LQ.entry[lq_index].virtual_address = ROB.entry[rob_index].source_memory[data_index];
    LQ.entry[lq_index].ip = ROB.entry[rob_index].ip;
    LQ.entry[lq_index].data_index = data_index;
    LQ.entry[lq_index].rob_index = rob_index;
    LQ.entry[lq_index].asid[0] = ROB.entry[rob_index].asid[0];
    LQ.entry[lq_index].asid[1] = ROB.entry[rob_index].asid[1];
    LQ.entry[lq_index].event_cycle = current_cycle + SCHEDULING_LATENCY;
    LQ.occupancy++;

    // check RAW dependency
    int prior = rob_index - 1;
    if (prior < 0)
        prior = ROB.SIZE - 1;

    if (rob_index != ROB.head) {
        if ((int)ROB.head <= prior) {
            for (int i=prior; i>=(int)ROB.head; i--) {
                if (LQ.entry[lq_index].producer_id != UINT64_MAX)
                    break;

		mem_RAW_dependency(i, rob_index, data_index, lq_index);
            }
        }
        else {
            for (int i=prior; i>=0; i--) {
                if (LQ.entry[lq_index].producer_id != UINT64_MAX)
                    break;

		mem_RAW_dependency(i, rob_index, data_index, lq_index);
            }
            for (int i=ROB.SIZE-1; i>=(int)ROB.head; i--) { 
                if (LQ.entry[lq_index].producer_id != UINT64_MAX)
                    break;

		mem_RAW_dependency(i, rob_index, data_index, lq_index);
            }
        }
    }

    // check
    // 1) if store-to-load forwarding is possible
    // 2) if there is WAR that are not correctly executed
    uint32_t forwarding_index = SQ.SIZE;
    for (uint32_t i=0; i<SQ.SIZE; i++) {

        // skip empty slot
        if (SQ.entry[i].virtual_address == 0)
            continue;

        // forwarding should be done by the SQ entry that holds the same producer_id from RAW dependency check
        if (SQ.entry[i].virtual_address == LQ.entry[lq_index].virtual_address) { // store-to-load forwarding check

            // forwarding store is in the SQ
            if ((rob_index != ROB.head) && (LQ.entry[lq_index].producer_id == SQ.entry[i].instr_id)) { // RAW
                forwarding_index = i;
                break; // should be break
            }

            if ((LQ.entry[lq_index].producer_id == UINT64_MAX) && (LQ.entry[lq_index].instr_id <= SQ.entry[i].instr_id)) { // WAR 
                // a load is about to be added in the load queue and we found a store that is 
                // "logically later in the program order but already executed" => this is not correctly executed WAR
                // due to out-of-order execution, this case is possible, for example
                // 1) application is load intensive and load queue is full
                // 2) we have loads that can't be added in the load queue
                // 3) subsequent stores logically behind in the program order are added in the store queue first

                // thanks to the store buffer, data is not written back to the memory system until retirement
                // also due to in-order retirement, this "already executed store" cannot be retired until we finish the prior load instruction 
                // if we detect WAR when a load is added in the load queue, just let the load instruction to access the memory system
                // no need to mark any dependency because this is actually WAR not RAW

                // do not forward data from the store queue since this is WAR
                // just read correct data from data cache

                LQ.entry[lq_index].physical_address = 0;
                LQ.entry[lq_index].translated = 0;
                LQ.entry[lq_index].fetched = 0;
                
                DP(if(warmup_complete[cpu]) {
                cout << "[LQ] " << __func__ << " instr_id: " << LQ.entry[lq_index].instr_id << " reset fetched: " << +LQ.entry[lq_index].fetched;
                cout << " to obey WAR store instr_id: " << SQ.entry[i].instr_id << " cycle: " << current_cycle << endl; });
            }
        }
    }

    if (forwarding_index != SQ.SIZE) { // we have a store-to-load forwarding

        if ((SQ.entry[forwarding_index].fetched == COMPLETED) && (SQ.entry[forwarding_index].event_cycle <= current_cycle)) {
            LQ.entry[lq_index].physical_address = (SQ.entry[forwarding_index].physical_address & ~(uint64_t) ((1 << LOG2_BLOCK_SIZE) - 1)) | (LQ.entry[lq_index].virtual_address & ((1 << LOG2_BLOCK_SIZE) - 1));
            LQ.entry[lq_index].translated = COMPLETED;
            LQ.entry[lq_index].fetched = COMPLETED;

            uint32_t fwr_rob_index = LQ.entry[lq_index].rob_index;
            ROB.entry[fwr_rob_index].num_mem_ops--;
            ROB.entry[fwr_rob_index].event_cycle = current_cycle;
            if (ROB.entry[fwr_rob_index].num_mem_ops < 0) {
                cerr << "instr_id: " << ROB.entry[fwr_rob_index].instr_id << endl;
                assert(0);
            }
            if (ROB.entry[fwr_rob_index].num_mem_ops == 0)
                inflight_mem_executions++;

            DP(if(warmup_complete[cpu]) {
            cout << "[LQ] " << __func__ << " instr_id: " << LQ.entry[lq_index].instr_id << hex;
            cout << " full_addr: " << LQ.entry[lq_index].physical_address << dec << " is forwarded by store instr_id: ";
            cout << SQ.entry[forwarding_index].instr_id << " remain_num_ops: " << ROB.entry[fwr_rob_index].num_mem_ops << " cycle: " << current_cycle << endl; });

            release_load_queue(lq_index);
        }
        else
            ; // store is not executed yet, forwarding will be handled by execute_store()
    }

    // succesfully added to the load queue
    ROB.entry[rob_index].source_added[data_index] = 1;

    if (LQ.entry[lq_index].virtual_address && (LQ.entry[lq_index].producer_id == UINT64_MAX)) { // not released and no forwarding
        RTL0.push(lq_index);

        DP (if (warmup_complete[cpu]) {
                std::cout << "[RTL0] " << __func__ << " instr_id: " << LQ.entry[lq_index].instr_id << " rob_index: " << LQ.entry[lq_index].rob_index << " is added to RTL0" << std::endl; });
    }

    DP(if(warmup_complete[cpu]) {
    cout << "[LQ] " << __func__ << " instr_id: " << LQ.entry[lq_index].instr_id;
    cout << " is added in the LQ address: " << hex << LQ.entry[lq_index].virtual_address << dec << " translated: " << +LQ.entry[lq_index].translated;
    cout << " fetched: " << +LQ.entry[lq_index].fetched << " index: " << lq_index << " occupancy: " << LQ.occupancy << " cycle: " << current_cycle << endl; });
}

void O3_CPU::mem_RAW_dependency(uint32_t prior, uint32_t current, uint32_t data_index, uint32_t lq_index)
{
    for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
        if (ROB.entry[prior].destination_memory[i] == 0)
            continue;

        if (ROB.entry[prior].destination_memory[i] == ROB.entry[current].source_memory[data_index]) { //  store-to-load forwarding check

            // we need to mark this dependency in the ROB since the producer might not be added in the store queue yet
            ROB.entry[prior].memory_instrs_depend_on_me.insert (current);   // this load cannot be executed until the prior store gets executed
            ROB.entry[prior].is_producer = 1;
            LQ.entry[lq_index].producer_id = ROB.entry[prior].instr_id; 
            LQ.entry[lq_index].translated = INFLIGHT;

            DP (if(warmup_complete[cpu]) {
            cout << "[LQ] " << __func__ << " RAW producer instr_id: " << ROB.entry[prior].instr_id << " consumer_id: " << ROB.entry[current].instr_id << " lq_index: " << lq_index;
            cout << hex << " address: " << ROB.entry[prior].destination_memory[i] << dec << endl; });

            return;
        }
    }
}

void O3_CPU::add_store_queue(uint32_t rob_index, uint32_t data_index)
{
    uint32_t sq_index = SQ.tail;
#ifdef SANITY_CHECK
    if (SQ.entry[sq_index].virtual_address)
        assert(0);
#endif

    /*
    // search for an empty slot 
    uint32_t sq_index = SQ.SIZE;
    for (uint32_t i=0; i<SQ.SIZE; i++) {
        if (SQ.entry[i].virtual_address == 0) {
            sq_index = i;
            break;
        }
    }

    // sanity check
    if (sq_index == SQ.SIZE) {
        cerr << "instr_id: " << ROB.entry[rob_index].instr_id << " no empty slot in the store queue!!!" << endl;
        assert(0);
    }
    */

    // add it to the store queue
    ROB.entry[rob_index].sq_index[data_index] = sq_index;
    SQ.entry[sq_index].instr_id = ROB.entry[rob_index].instr_id;
    SQ.entry[sq_index].virtual_address = ROB.entry[rob_index].destination_memory[data_index];
    SQ.entry[sq_index].ip = ROB.entry[rob_index].ip;
    SQ.entry[sq_index].data_index = data_index;
    SQ.entry[sq_index].rob_index = rob_index;
    SQ.entry[sq_index].asid[0] = ROB.entry[rob_index].asid[0];
    SQ.entry[sq_index].asid[1] = ROB.entry[rob_index].asid[1];
    SQ.entry[sq_index].event_cycle = current_cycle + SCHEDULING_LATENCY;

    SQ.occupancy++;
    SQ.tail++;
    if (SQ.tail == SQ.SIZE)
        SQ.tail = 0;

    // succesfully added to the store queue
    ROB.entry[rob_index].destination_added[data_index] = 1;

    STA.pop();

    RTS0.push(sq_index);

    DP(if(warmup_complete[cpu]) {
            std::cout << "[SQ] " << __func__ << " instr_id: " << SQ.entry[sq_index].instr_id;
            std::cout << " is added in the SQ translated: " << +SQ.entry[sq_index].translated << " fetched: " << +SQ.entry[sq_index].fetched << " is_producer: " << +ROB.entry[rob_index].is_producer;
            std::cout << " cycle: " << current_cycle << std::endl; });
}

void O3_CPU::operate_lsq()
{
    // handle store
    uint32_t store_issued = 0, num_iteration = 0;

    while (store_issued < SQ_WIDTH) {
        if (!RTS0.empty()) {
            uint32_t sq_index = RTS0.front();
            if (SQ.entry[sq_index].event_cycle <= current_cycle) {

                // add it to DTLB
                PACKET data_packet;

                data_packet.fill_level = FILL_L1;
                data_packet.cpu = cpu;
                if (knob_cloudsuite)
                    data_packet.address = ((SQ.entry[sq_index].virtual_address >> LOG2_PAGE_SIZE) << 9) | SQ.entry[sq_index].asid[1];
                else
                    data_packet.address = SQ.entry[sq_index].virtual_address >> LOG2_PAGE_SIZE;
                data_packet.full_addr = SQ.entry[sq_index].virtual_address;
                data_packet.instr_id = SQ.entry[sq_index].instr_id;
                data_packet.rob_index = SQ.entry[sq_index].rob_index;
                data_packet.ip = SQ.entry[sq_index].ip;
                data_packet.type = RFO;
                data_packet.asid[0] = SQ.entry[sq_index].asid[0];
                data_packet.asid[1] = SQ.entry[sq_index].asid[1];
                data_packet.to_return = {&DTLB_bus};
                data_packet.sq_index_depend_on_me = {sq_index};

                DP (if (warmup_complete[cpu]) {
                        std::cout << "[RTS0] " << __func__ << " instr_id: " << SQ.entry[sq_index].instr_id << " rob_index: " << SQ.entry[sq_index].rob_index << " is popped from to RTS0" << std::endl; });

                int rq_index = DTLB_bus.lower_level->add_rq(&data_packet);

                if (rq_index == -2)
                    break; 
                else 
                    SQ.entry[sq_index].translated = INFLIGHT;

                RTS0.pop();

                store_issued++;
            }
        }
        else {
            break;
        }

        num_iteration++;
        if (num_iteration == (SQ.SIZE-1))
            break;
    }

    num_iteration = 0;
    while (store_issued < SQ_WIDTH) {
        if (!RTS1.empty()) {
            uint32_t sq_index = RTS1.front();
            if (SQ.entry[sq_index].event_cycle <= current_cycle) {
                execute_store(SQ.entry[sq_index].rob_index, sq_index, SQ.entry[sq_index].data_index);

                RTS1.pop();

                store_issued++;
            }
        }
        else {
            break;
        }

        num_iteration++;
        if (num_iteration == (SQ.SIZE-1))
            break;
    }

    unsigned load_issued = 0;
    num_iteration = 0;
    while (load_issued < LQ_WIDTH) {
        if (!RTL0.empty()) {
            uint32_t lq_index = RTL0.front();
            if (LQ.entry[lq_index].event_cycle <= current_cycle) {

                // add it to DTLB
                PACKET data_packet;
                data_packet.fill_level = FILL_L1;
                data_packet.cpu = cpu;
                if (knob_cloudsuite)
                    data_packet.address = ((LQ.entry[lq_index].virtual_address >> LOG2_PAGE_SIZE) << 9) | LQ.entry[lq_index].asid[1];
                else
                    data_packet.address = LQ.entry[lq_index].virtual_address >> LOG2_PAGE_SIZE;
                data_packet.full_addr = LQ.entry[lq_index].virtual_address;
                data_packet.instr_id = LQ.entry[lq_index].instr_id;
                data_packet.rob_index = LQ.entry[lq_index].rob_index;
                data_packet.ip = LQ.entry[lq_index].ip;
                data_packet.type = LOAD;
                data_packet.asid[0] = LQ.entry[lq_index].asid[0];
                data_packet.asid[1] = LQ.entry[lq_index].asid[1];
                data_packet.to_return = {&DTLB_bus};
                data_packet.lq_index_depend_on_me = {lq_index};

                DP (if (warmup_complete[cpu]) {
                        std::cout << "[RTL0] " << __func__ << " instr_id: " << LQ.entry[lq_index].instr_id << " rob_index: " << LQ.entry[lq_index].rob_index << " is popped to RTL0" << std::endl; });

                int rq_index = DTLB_bus.lower_level->add_rq(&data_packet);

                if (rq_index == -2)
                    break; // break here
                else  
                    LQ.entry[lq_index].translated = INFLIGHT;

                RTL0.pop();

                load_issued++;
            }
        }
        else {
            break;
        }

        num_iteration++;
        if (num_iteration == (LQ.SIZE-1))
            break;
    }

    num_iteration = 0;
    while (load_issued < LQ_WIDTH) {
        if (!RTL1.empty()) {
            uint32_t lq_index = RTL1.front();
            if (LQ.entry[lq_index].event_cycle <= current_cycle) {
                int rq_index = execute_load(LQ.entry[lq_index].rob_index, lq_index, LQ.entry[lq_index].data_index);

                if (rq_index != -2) {
                    RTL1.pop();

                    load_issued++;
                }
            }
        }
        else {
            break;
        }

        num_iteration++;
        if (num_iteration == (LQ.SIZE-1))
            break;
    }
}

void O3_CPU::execute_store(uint32_t rob_index, uint32_t sq_index, uint32_t data_index)
{
    SQ.entry[sq_index].fetched = COMPLETED;

    ROB.entry[rob_index].num_mem_ops--;
    ROB.entry[rob_index].event_cycle = current_cycle;
    if (ROB.entry[rob_index].num_mem_ops < 0) {
        cerr << "instr_id: " << ROB.entry[rob_index].instr_id << endl;
        assert(0);
    }
    if (ROB.entry[rob_index].num_mem_ops == 0)
        inflight_mem_executions++;

    DP (if (warmup_complete[cpu]) {
    cout << "[SQ1] " << __func__ << " instr_id: " << SQ.entry[sq_index].instr_id << hex;
    cout << " full_address: " << SQ.entry[sq_index].physical_address << dec << " remain_mem_ops: " << ROB.entry[rob_index].num_mem_ops;
    cout << " event_cycle: " << SQ.entry[sq_index].event_cycle << endl; });

    // resolve RAW dependency after DTLB access
    // check if this store has dependent loads
    if (ROB.entry[rob_index].is_producer) {
	ITERATE_SET(dependent,ROB.entry[rob_index].memory_instrs_depend_on_me, ROB.SIZE) {
            // check if dependent loads are already added in the load queue
            for (uint32_t j=0; j<NUM_INSTR_SOURCES; j++) { // which one is dependent?
                if (ROB.entry[dependent].source_memory[j] && ROB.entry[dependent].source_added[j]) {
                    if (ROB.entry[dependent].source_memory[j] == SQ.entry[sq_index].virtual_address) { // this is required since a single instruction can issue multiple loads

                        // now we can resolve RAW dependency
                        uint32_t lq_index = ROB.entry[dependent].lq_index[j];
#ifdef SANITY_CHECK
                        if (lq_index >= LQ.SIZE)
                            assert(0);
                        if (LQ.entry[lq_index].producer_id != SQ.entry[sq_index].instr_id) {
                            cerr << "[SQ2] " << __func__ << " lq_index: " << lq_index << " producer_id: " << LQ.entry[lq_index].producer_id;
                            cerr << " does not match to the store instr_id: " << SQ.entry[sq_index].instr_id << endl;
                            assert(0);
                        }
#endif
                        // update correspodning LQ entry
                        LQ.entry[lq_index].physical_address = (SQ.entry[sq_index].physical_address & ~(uint64_t) ((1 << LOG2_BLOCK_SIZE) - 1)) | (LQ.entry[lq_index].virtual_address & ((1 << LOG2_BLOCK_SIZE) - 1));
                        LQ.entry[lq_index].translated = COMPLETED;
                        LQ.entry[lq_index].fetched = COMPLETED;
                        LQ.entry[lq_index].event_cycle = current_cycle;

                        uint32_t fwr_rob_index = LQ.entry[lq_index].rob_index;
                        ROB.entry[fwr_rob_index].num_mem_ops--;
                        ROB.entry[fwr_rob_index].event_cycle = current_cycle;
#ifdef SANITY_CHECK
                        if (ROB.entry[fwr_rob_index].num_mem_ops < 0) {
                            cerr << "instr_id: " << ROB.entry[fwr_rob_index].instr_id << endl;
                            assert(0);
                        }
#endif
                        if (ROB.entry[fwr_rob_index].num_mem_ops == 0)
                            inflight_mem_executions++;

                        DP(if(warmup_complete[cpu]) {
                                std::cout << "[LQ3] " << __func__ << " instr_id: " << LQ.entry[lq_index].instr_id << std::hex;
                                std::cout << " full_addr: " << LQ.entry[lq_index].physical_address << std::dec << " is forwarded by store instr_id: ";
                                std::cout << SQ.entry[sq_index].instr_id << " remain_num_ops: " << ROB.entry[fwr_rob_index].num_mem_ops << " cycle: " << current_cycle << std::endl; });

                        release_load_queue(lq_index);

                        // clear dependency bit
                        if (j == (NUM_INSTR_SOURCES-1))
                            ROB.entry[rob_index].memory_instrs_depend_on_me.insert (dependent);
                    }
                }
            }
        }
    }
}

int O3_CPU::execute_load(uint32_t rob_index, uint32_t lq_index, uint32_t data_index)
{
    // add it to L1D
    PACKET data_packet;
    data_packet.fill_level = FILL_L1;
    data_packet.cpu = cpu;
    data_packet.address = LQ.entry[lq_index].physical_address >> LOG2_BLOCK_SIZE;
    data_packet.full_addr = LQ.entry[lq_index].physical_address;
    data_packet.v_address = LQ.entry[lq_index].virtual_address >> LOG2_BLOCK_SIZE;
    data_packet.full_v_addr = LQ.entry[lq_index].virtual_address;
    data_packet.instr_id = LQ.entry[lq_index].instr_id;
    data_packet.rob_index = LQ.entry[lq_index].rob_index;
    data_packet.ip = LQ.entry[lq_index].ip;
    data_packet.type = LOAD;
    data_packet.asid[0] = LQ.entry[lq_index].asid[0];
    data_packet.asid[1] = LQ.entry[lq_index].asid[1];
    data_packet.to_return = {&L1D_bus};
    data_packet.lq_index_depend_on_me = {lq_index};

    int rq_index = L1D_bus.lower_level->add_rq(&data_packet);

    if (rq_index == -2)
        return rq_index;
    else 
        LQ.entry[lq_index].fetched = INFLIGHT;

    return rq_index;
}

uint32_t O3_CPU::complete_execution(uint32_t rob_index)
{
    if (ROB.entry[rob_index].is_memory == 0) {
        if ((ROB.entry[rob_index].executed == INFLIGHT) && (ROB.entry[rob_index].event_cycle <= current_cycle)) {

            ROB.entry[rob_index].executed = COMPLETED; 
            inflight_reg_executions--;
            completed_executions++;

            if (ROB.entry[rob_index].reg_RAW_producer)
                reg_RAW_release(rob_index);

            if (ROB.entry[rob_index].branch_mispredicted)
	      {
              fetch_resume_cycle = current_cycle + BRANCH_MISPREDICT_PENALTY;
	      }

            DP(if(warmup_complete[cpu]) {
            cout << "[ROB] " << __func__ << " instr_id: " << ROB.entry[rob_index].instr_id;
            cout << " branch_mispredicted: " << +ROB.entry[rob_index].branch_mispredicted << " fetch_stall: " << +fetch_stall;
            cout << " event: " << ROB.entry[rob_index].event_cycle << endl; });

	    return 1;
        }
    }
    else {
        if (ROB.entry[rob_index].num_mem_ops == 0) {
            if ((ROB.entry[rob_index].executed == INFLIGHT) && (ROB.entry[rob_index].event_cycle <= current_cycle)) {

	      ROB.entry[rob_index].executed = COMPLETED;
                inflight_mem_executions--;
                completed_executions++;
                
                if (ROB.entry[rob_index].reg_RAW_producer)
                    reg_RAW_release(rob_index);

                if (ROB.entry[rob_index].branch_mispredicted)
		  {
              fetch_resume_cycle = current_cycle + BRANCH_MISPREDICT_PENALTY;
		  }

                DP(if(warmup_complete[cpu]) {
                        std::cout << "[ROB] " << __func__ << " instr_id: " << ROB.entry[rob_index].instr_id;
                        std::cout << " is_memory: " << +ROB.entry[rob_index].is_memory << " branch_mispredicted: " << +ROB.entry[rob_index].branch_mispredicted;
                        std::cout << " fetch_stall: " << +fetch_stall << " event: " << ROB.entry[rob_index].event_cycle << " current: " << current_cycle << std::endl; });

		return 1;
            }
        }
    }

    return 0;
}

void O3_CPU::reg_RAW_release(uint32_t rob_index)
{
    // if (!ROB.entry[rob_index].registers_instrs_depend_on_me.empty()) 

    ITERATE_SET(i,ROB.entry[rob_index].registers_instrs_depend_on_me, ROB.SIZE) {
        for (uint32_t j=0; j<NUM_INSTR_SOURCES; j++) {
            if (ROB.entry[rob_index].registers_index_depend_on_me[j].search (i)) {
                ROB.entry[i].num_reg_dependent--;

                if (ROB.entry[i].num_reg_dependent == 0) {
                    ROB.entry[i].reg_ready = 1;
                    if (ROB.entry[i].is_memory)
                        ROB.entry[i].scheduled = INFLIGHT;
                    else {
                        ROB.entry[i].scheduled = COMPLETED;

#ifdef SANITY_CHECK
                        assert(ready_to_execute.size() <= ROB.SIZE);
#endif
                        // remember this rob_index in the Ready-To-Execute array 0
                        ready_to_execute.push(i);

                        DP (if (warmup_complete[cpu]) {
                                std::cout << "[ready_to_execute] " << __func__ << " instr_id: " << ROB.entry[i].instr_id << " rob_index: " << i << " is added to ready_to_execute" << std::endl; });

                    }
                }

                DP (if (warmup_complete[cpu]) {
                        std::cout << "[ROB] " << __func__ << " instr_id: " << ROB.entry[rob_index].instr_id << " releases instr_id: ";
                        std::cout << ROB.entry[i].instr_id << " reg_index: " << +ROB.entry[i].source_registers[j] << " num_reg_dependent: " << ROB.entry[i].num_reg_dependent << " cycle: " << current_cycle << std::endl; });
            }
        }
    }
}

void O3_CPU::complete_inflight_instruction()
{
    // update ROB entries with completed executions
    if ((inflight_reg_executions > 0) || (inflight_mem_executions > 0)) {
        uint32_t instrs_executed = 0;
        for (uint32_t i=ROB.head, count=0; count<ROB.occupancy; i=(i+1==ROB.SIZE) ? 0 : i+1, count++) {
	    if(instrs_executed >= EXEC_WIDTH)
	    {
	        break;
	    }
	    instrs_executed += complete_execution(i);
	}
    }
}

void O3_CPU::handle_memory_return()
{
  // Instruction Memory

  std::size_t available_fetch_bandwidth = FETCH_WIDTH;
  std::size_t to_read = static_cast<CACHE*>(ITLB_bus.lower_level)->MAX_READ;

  while (available_fetch_bandwidth > 0 && to_read > 0 && !ITLB_bus.PROCESSED.empty())
    {
        PACKET &itlb_entry = ITLB_bus.PROCESSED.front();

      // mark the appropriate instructions in the IFETCH_BUFFER as translated and ready to fetch
      while (available_fetch_bandwidth > 0 && !itlb_entry.instr_depend_on_me.empty())
      {
          auto it = itlb_entry.instr_depend_on_me.front();
          if ((it->ip >> LOG2_PAGE_SIZE) == (itlb_entry.address) && it->translated != 0)
          {
              it->translated = COMPLETED;
              // recalculate a physical address for this cache line based on the translated physical page address
              it->instruction_pa = (itlb_entry.data << LOG2_PAGE_SIZE) | (it->ip & ((1 << LOG2_PAGE_SIZE) - 1));

              available_fetch_bandwidth--;
          }

          itlb_entry.instr_depend_on_me.pop_front();
      }


      // remove this entry if we have serviced all of its instructions
      if (itlb_entry.instr_depend_on_me.empty())
      {
          ITLB_bus.PROCESSED.pop_front();
      }
      --to_read;
  }

  available_fetch_bandwidth = FETCH_WIDTH;
  to_read = static_cast<CACHE*>(L1I_bus.lower_level)->MAX_READ;

  while (available_fetch_bandwidth > 0 && to_read > 0 && !L1I_bus.PROCESSED.empty())
  {
      PACKET &l1i_entry = L1I_bus.PROCESSED.front();

      // this is the L1I cache, so instructions are now fully fetched, so mark them as such
      while (available_fetch_bandwidth > 0 && !l1i_entry.instr_depend_on_me.empty())
      {
          auto it = l1i_entry.instr_depend_on_me.front();
          if ((it->instruction_pa >> LOG2_BLOCK_SIZE) == (l1i_entry.address) && it->fetched != 0 && it->translated == COMPLETED)
          {
              it->fetched = COMPLETED;
              available_fetch_bandwidth--;
          }

          l1i_entry.instr_depend_on_me.pop_front();
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

	  for (auto sq_merged : dtlb_entry.sq_index_depend_on_me)
	    {
	      SQ.entry[sq_merged].physical_address = (dtlb_entry.data << LOG2_PAGE_SIZE) | (SQ.entry[sq_merged].virtual_address & ((1 << LOG2_PAGE_SIZE) - 1)); // translated address
	      SQ.entry[sq_merged].translated = COMPLETED;
	      SQ.entry[sq_merged].event_cycle = current_cycle;

          RTS1.push(sq_merged);
	    }

	  for (auto lq_merged : dtlb_entry.lq_index_depend_on_me)
	    {
	      LQ.entry[lq_merged].physical_address = (dtlb_entry.data << LOG2_PAGE_SIZE) | (LQ.entry[lq_merged].virtual_address & ((1 << LOG2_PAGE_SIZE) - 1)); // translated address
	      LQ.entry[lq_merged].translated = COMPLETED;
	      LQ.entry[lq_merged].event_cycle = current_cycle;

          RTL1.push(lq_merged);
	    }

	  ROB.entry[dtlb_entry.rob_index].event_cycle = current_cycle;

	  // remove this entry
	  DTLB_bus.PROCESSED.pop_front();
      --to_read;
    }

  to_read = static_cast<CACHE*>(L1D_bus.lower_level)->MAX_READ;
  while (to_read > 0 && !L1D_bus.PROCESSED.empty())
	{ // L1D
	  PACKET &l1d_entry = L1D_bus.PROCESSED.front();

	  for (auto merged : l1d_entry.lq_index_depend_on_me)
	    {
	      LQ.entry[merged].fetched = COMPLETED;
	      LQ.entry[merged].event_cycle = current_cycle;
	      ROB.entry[LQ.entry[merged].rob_index].num_mem_ops--;
	      ROB.entry[LQ.entry[merged].rob_index].event_cycle = current_cycle;

	      if (ROB.entry[LQ.entry[merged].rob_index].num_mem_ops == 0)
		inflight_mem_executions++;

	      release_load_queue(merged);
	    }

	  // remove this entry
	  L1D_bus.PROCESSED.pop_front();
      --to_read;;
    }
}

void O3_CPU::release_load_queue(uint32_t lq_index)
{
    // release LQ entries
    DP ( if (warmup_complete[cpu]) {
    cout << "[LQ] " << __func__ << " instr_id: " << LQ.entry[lq_index].instr_id << " releases lq_index: " << lq_index;
    cout << hex << " full_addr: " << LQ.entry[lq_index].physical_address << dec << endl; });

    LSQ_ENTRY empty_entry;
    LQ.entry[lq_index] = empty_entry;
    LQ.occupancy--;
}

void O3_CPU::retire_rob()
{
    unsigned retire_bandwidth = RETIRE_WIDTH;

    while (retire_bandwidth > 0 && ROB.entry[ROB.head].ip != 0 && (ROB.entry[ROB.head].executed == COMPLETED))
    {
        for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
            if (ROB.entry[ROB.head].destination_memory[i]) {

                PACKET data_packet;
                uint32_t sq_index = ROB.entry[ROB.head].sq_index[i];

                // sq_index and rob_index are no longer available after retirement
                // but we pass this information to avoid segmentation fault
                data_packet.fill_level = FILL_L1;
                data_packet.cpu = cpu;
                data_packet.address = SQ.entry[sq_index].physical_address >> LOG2_BLOCK_SIZE;
                data_packet.full_addr = SQ.entry[sq_index].physical_address;
                data_packet.v_address = SQ.entry[sq_index].virtual_address >> LOG2_BLOCK_SIZE;
                data_packet.full_v_addr = SQ.entry[sq_index].virtual_address;
                data_packet.instr_id = SQ.entry[sq_index].instr_id;
                data_packet.rob_index = SQ.entry[sq_index].rob_index;
                data_packet.ip = SQ.entry[sq_index].ip;
                data_packet.type = RFO;
                data_packet.asid[0] = SQ.entry[sq_index].asid[0];
                data_packet.asid[1] = SQ.entry[sq_index].asid[1];

                auto result = L1D_bus.lower_level->add_wq(&data_packet);
                if (result != -2)
                    ROB.entry[ROB.head].destination_memory[i] = 0;
                else
                    return;
            }
        }

        // release SQ entries
        for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
            if (ROB.entry[ROB.head].sq_index[i] != UINT32_MAX) {
                uint32_t sq_index = ROB.entry[ROB.head].sq_index[i];

                DP ( if (warmup_complete[cpu]) {
                        cout << "[SQ] " << __func__ << " instr_id: " << ROB.entry[ROB.head].instr_id << " releases sq_index: " << sq_index;
                        cout << hex << " address: " << (SQ.entry[sq_index].physical_address>>LOG2_BLOCK_SIZE);
                        cout << " full_addr: " << SQ.entry[sq_index].physical_address << dec << endl; });

                LSQ_ENTRY empty_entry;
                SQ.entry[sq_index] = empty_entry;

                SQ.occupancy--;
                SQ.head++;
                if (SQ.head == SQ.SIZE)
                    SQ.head = 0;
            }
        }

        // release ROB entry
        DP ( if (warmup_complete[cpu]) {
                cout << "[ROB] " << __func__ << " instr_id: " << ROB.entry[ROB.head].instr_id << " is retired" << endl; });

        ooo_model_instr empty_entry;
        ROB.entry[ROB.head] = empty_entry;

        ROB.head++;
        if (ROB.head == ROB.SIZE)
            ROB.head = 0;
        ROB.occupancy--;
        completed_executions--;
        num_retired++;
        retire_bandwidth--;
    }
}

void CacheBus::return_data(PACKET *packet)
{
    if (packet->type != PREFETCH)
    {
        PROCESSED.push_back(*packet);
    }
}

