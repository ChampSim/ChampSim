#include <algorithm>
#include <vector>

#include "ooo_cpu.h"
#include "instruction.h"
#include "set.h"
#include "vmem.h"

// out-of-order core
extern std::vector<O3_CPU> ooo_cpu;
uint64_t current_core_cycle[NUM_CPUS];

extern uint8_t warmup_complete[NUM_CPUS];
extern uint8_t knob_cloudsuite;
extern uint8_t MAX_INSTR_DESTINATIONS;

extern VirtualMemory vmem;

void O3_CPU::initialize_core()
{

}

uint32_t O3_CPU::init_instruction(ooo_model_instr arch_instr)
{
    // actual processors do not work like this but for easier implementation,
    // we read instruction traces and virtually add them in the ROB
    // note that these traces are not yet translated and fetched

    if (instrs_to_read_this_cycle == 0)
        instrs_to_read_this_cycle = std::min((std::size_t)FETCH_WIDTH, IFETCH_BUFFER.size() - IFETCH_BUFFER.occupancy());

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
                if (STA[STA_tail] < UINT64_MAX)
                {
                    if (STA_head != STA_tail)
                        assert(0);
                }
#endif
                STA[STA_tail] = instr_unique_id;
                STA_tail++;

                if (STA_tail == STA_SIZE)
                    STA_tail = 0;
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

    arch_instr.event_cycle = current_core_cycle[cpu];

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

    return instrs_to_read_this_cycle;
}

uint32_t O3_CPU::check_rob(uint64_t instr_id)
{
    if ((ROB.head == ROB.tail) && ROB.occupancy == 0)
        return ROB.SIZE;

    for (uint32_t i=ROB.head, count=0; count<ROB.occupancy; i=(i+1==ROB.SIZE) ? 0 : i+1, count++) {
        if (ROB.entry[i].instr_id == instr_id) {
            DP ( if (warmup_complete[ROB.cpu]) {
            cout << "[ROB] " << __func__ << " same instr_id: " << ROB.entry[i].instr_id;
            cout << " rob_index: " << i << endl; });
            return i;
        }
    }

    cerr << "[ROB_ERROR] " << __func__ << " does not have any matching index! ";
    cerr << " instr_id: " << instr_id << endl;
    assert(0);

    return ROB.SIZE;
}

void O3_CPU::check_dib()
{
    // scan through IFETCH_BUFFER to find instructions that hit in the decoded instruction buffer
    auto end = std::min(IFETCH_BUFFER.end(), std::next(IFETCH_BUFFER.begin(), FETCH_WIDTH));
    for (auto it = IFETCH_BUFFER.begin(); it != end; ++it)
        do_check_dib(*it);
}

void O3_CPU::do_check_dib(ooo_model_instr &instr)
{
    // Check DIB to see if we recently fetched this line
    dib_t::value_type &dib_set = DIB[(instr.ip >> LOG2_DIB_WINDOW_SIZE) % DIB_SET];
    auto way = std::find_if(dib_set.begin(), dib_set.end(), eq_addr<dib_entry_t>(instr.ip, LOG2_DIB_WINDOW_SIZE));
    if (way != dib_set.end())
    {
        // The cache line is in the L0, so we can mark this as complete
        instr.translated = COMPLETED;
        instr.fetched = COMPLETED;

        // Also mark it as decoded
        instr.decoded = COMPLETED;

        // It can be acted on immediately
        instr.event_cycle = current_core_cycle[cpu];

        // Update LRU
        std::for_each(dib_set.begin(), dib_set.end(), lru_updater<dib_entry_t>(way));
    }
}

void O3_CPU::translate_fetch()
{
    if (IFETCH_BUFFER.empty())
        return;

    // scan through IFETCH_BUFFER to find instructions that need to be translated
    auto itlb_req_begin = std::find_if(IFETCH_BUFFER.begin(), IFETCH_BUFFER.end(), [](const ooo_model_instr &x){ return !x.translated; });
    uint64_t find_addr = itlb_req_begin->ip;
    auto itlb_req_end   = std::find_if(itlb_req_begin, IFETCH_BUFFER.end(), [find_addr](const ooo_model_instr &x){ return (find_addr >> LOG2_PAGE_SIZE) != (x.ip >> LOG2_PAGE_SIZE);});
    if (itlb_req_end != IFETCH_BUFFER.end() || itlb_req_begin == IFETCH_BUFFER.begin())
    {
        do_translate_fetch(itlb_req_begin, itlb_req_end);
    }
}

void O3_CPU::do_translate_fetch(champsim::circular_buffer<ooo_model_instr>::iterator begin, champsim::circular_buffer<ooo_model_instr>::iterator end)
{
    // begin process of fetching this instruction by sending it to the ITLB
    // add it to the ITLB's read queue
    PACKET trace_packet;
    trace_packet.fill_level = FILL_L1;
    trace_packet.cpu = cpu;
    trace_packet.address = begin->ip >> LOG2_PAGE_SIZE;
    trace_packet.full_addr = begin->ip;
    trace_packet.instr_id = begin->instr_id;
    trace_packet.ip = begin->ip;
    trace_packet.type = LOAD;
    trace_packet.asid[0] = 0;
    trace_packet.asid[1] = 0;
    trace_packet.event_cycle = current_core_cycle[cpu];
    trace_packet.to_return = {&ITLB_bus};
    for (; begin != end; ++begin)
        trace_packet.instr_depend_on_me.push_back(begin);

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

void O3_CPU::fetch_instruction()
{
  // if we had a branch mispredict, turn fetching back on after the branch mispredict penalty
  if((fetch_stall == 1) && (current_core_cycle[cpu] >= fetch_resume_cycle) && (fetch_resume_cycle != 0))
    {
      fetch_stall = 0;
      fetch_resume_cycle = 0;
    }

  if (IFETCH_BUFFER.empty())
      return;

      // fetch cache lines that were part of a translated page but not the cache line that initiated the translation
    auto l1i_req_begin = std::find_if(IFETCH_BUFFER.begin(), IFETCH_BUFFER.end(),
            [](const ooo_model_instr &x){ return x.translated == COMPLETED && !x.fetched; });
    uint64_t find_addr = l1i_req_begin->instruction_pa;
    auto l1i_req_end   = std::find_if(l1i_req_begin, IFETCH_BUFFER.end(),
            [find_addr](const ooo_model_instr &x){ return (find_addr >> LOG2_BLOCK_SIZE) != (x.instruction_pa >> LOG2_BLOCK_SIZE);});
    if (l1i_req_end != IFETCH_BUFFER.end() || l1i_req_begin == IFETCH_BUFFER.begin())
    {
        do_fetch_instruction(l1i_req_begin, l1i_req_end);
    }
}

void O3_CPU::do_fetch_instruction(champsim::circular_buffer<ooo_model_instr>::iterator begin, champsim::circular_buffer<ooo_model_instr>::iterator end)
{
    // add it to the L1-I's read queue
    PACKET fetch_packet;
    fetch_packet.fill_level = FILL_L1;
    fetch_packet.cpu = cpu;
    fetch_packet.address = begin->instruction_pa >> LOG2_BLOCK_SIZE;
    fetch_packet.data = begin->instruction_pa;
    fetch_packet.full_addr = begin->instruction_pa;
    fetch_packet.v_address = begin->ip >> LOG2_PAGE_SIZE;
    fetch_packet.full_v_addr = begin->ip;
    fetch_packet.instr_id = begin->instr_id;
    fetch_packet.ip = begin->ip;
    fetch_packet.type = LOAD; 
    fetch_packet.asid[0] = 0;
    fetch_packet.asid[1] = 0;
    fetch_packet.event_cycle = current_core_cycle[cpu];
    fetch_packet.to_return = {&L1I_bus};
    for (; begin != end; ++begin)
        fetch_packet.instr_depend_on_me.push_back(begin);

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

void O3_CPU::promote_to_decode()
{
    unsigned available_fetch_bandwidth = FETCH_WIDTH;
    while (available_fetch_bandwidth > 0 && !IFETCH_BUFFER.empty() && !DECODE_BUFFER.full() &&
            IFETCH_BUFFER.front().translated == COMPLETED && IFETCH_BUFFER.front().fetched == COMPLETED)
    {
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
    std::size_t available_decode_bandwidth = DECODE_WIDTH;

    // Send decoded instructions to dispatch
    while (available_decode_bandwidth > 0 && DECODE_BUFFER.has_ready() && !DISPATCH_BUFFER.full())
    {
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
		fetch_resume_cycle = current_core_cycle[cpu] + BRANCH_MISPREDICT_PENALTY;
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

void O3_CPU::do_dib_update(const ooo_model_instr &instr)
{
    // Search DIB to see if we need to add this instruction
    dib_t::value_type &dib_set = DIB[(instr.ip >> LOG2_DIB_WINDOW_SIZE) % DIB_SET];
    auto way = std::find_if(dib_set.begin(), dib_set.end(), eq_addr<dib_entry_t>(instr.ip, LOG2_DIB_WINDOW_SIZE));

    // If we did not find the entry in the DIB, find a victim
    if (way == dib_set.end())
    {
        way = std::max_element(dib_set.begin(), dib_set.end(), lru_comparator<dib_entry_t>());
        assert(way != dib_set.end());

        // update way
        way->valid = true;
        way->address = instr.ip;
    }

    std::for_each(dib_set.begin(), dib_set.end(), lru_updater<dib_entry_t>(way));
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
        ROB.entry[ROB.tail].event_cycle = current_core_cycle[cpu];

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
  if(pf_v_addr == 0)
    {
      cerr << "Cannot prefetch code line 0x0 !!!" << endl;
      assert(0);
    }
  
  L1I.pf_requested++;

  if (!L1I.PQ.full())
    {
      // magically translate prefetches
      uint64_t pf_pa = (vmem.va_to_pa(cpu, pf_v_addr) & (~((1 << LOG2_PAGE_SIZE) - 1))) | (pf_v_addr & ((1 << LOG2_PAGE_SIZE) - 1));

      PACKET pf_packet;
      pf_packet.fill_level = FILL_L1;
      pf_packet.pf_origin_level = FILL_L1;
      pf_packet.cpu = cpu;

      pf_packet.address = pf_pa >> LOG2_BLOCK_SIZE;
      pf_packet.full_addr = pf_pa;

      pf_packet.ip = pf_v_addr;
      pf_packet.type = PREFETCH;
      pf_packet.event_cycle = current_core_cycle[cpu];

      L1I_bus.lower_level->add_pq(&pf_packet);
      L1I.pf_issued++;
    
      return 1;
    }
  
 return 0;
}

// TODO: When should we update ROB.schedule_event_cycle?
// I. Instruction is fetched
// II. Instruction is completed
// III. Instruction is retired
void O3_CPU::schedule_instruction()
{
    if ((ROB.head == ROB.tail) && ROB.occupancy == 0)
        return;

    num_searched = 0;
    for (uint32_t i=ROB.head, count=0; count<ROB.occupancy; i=(i+1==ROB.SIZE) ? 0 : i+1, count++) {
        if ((ROB.entry[i].fetched != COMPLETED) || (ROB.entry[i].event_cycle > current_core_cycle[cpu]) || (num_searched >= SCHEDULER_SIZE))
            return;

        if (ROB.entry[i].scheduled == 0)
        {
            do_scheduling(i);

            if (ROB.entry[i].scheduled == COMPLETED && ROB.entry[i].num_reg_dependent == 0) {

                // remember this rob_index in the Ready-To-Execute array 1
                assert(ready_to_execute[ready_to_execute_tail] == ROB_SIZE);
                ready_to_execute[ready_to_execute_tail] = i;

                DP (if (warmup_complete[cpu]) {
                        std::cout << "[ready_to_execute] " << __func__ << " instr_id: " << ROB.entry[i].instr_id << " rob_index: " << i << " is added to ready_to_execute";
                        std::cout << " head: " << ready_to_execute_head << " tail: " << ready_to_execute_tail << std::endl; });

                ready_to_execute_tail++;
                if (ready_to_execute_tail == ROB_SIZE)
                    ready_to_execute_tail = 0;
            }
        }

        if(ROB.entry[i].executed == 0)
            num_searched++;
    }
}

void O3_CPU::do_scheduling(uint32_t rob_index)
{
    ooo_model_instr &rob_entry = ROB.entry[rob_index];

    // Mark register dependencies
    for (auto src_reg : rob_entry.source_registers) {
        if (src_reg) {
            std::size_t prior_idx = rob_index;
            while (prior_idx != ROB.head)
            {
                prior_idx = (prior_idx == 0) ? ROB.SIZE-1 : prior_idx-1;
                ooo_model_instr &prior = ROB.entry[prior_idx];
                if (prior.executed != COMPLETED) {
                    auto found = std::find(std::begin(prior.destination_registers), std::end(prior.destination_registers), src_reg);
                    if (found != std::end(prior.destination_registers)) {
                        prior.registers_instrs_depend_on_me.push_back(&rob_entry);
                        rob_entry.num_reg_dependent++;
                        break;
                    }
                }
            }
        }
    }

    if (rob_entry.is_memory)
        rob_entry.scheduled = INFLIGHT;
    else {
        rob_entry.scheduled = COMPLETED;

        // ADD LATENCY
        if (warmup_complete[cpu])
        {
            if (rob_entry.event_cycle < current_core_cycle[cpu])
                rob_entry.event_cycle = current_core_cycle[cpu] + SCHEDULING_LATENCY;
            else
                rob_entry.event_cycle += SCHEDULING_LATENCY;
        }
        else
        {
            if (rob_entry.event_cycle < current_core_cycle[cpu])
                rob_entry.event_cycle = current_core_cycle[cpu];
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
        if (ready_to_execute[ready_to_execute_head] < ROB_SIZE) {
            uint32_t exec_index = ready_to_execute[ready_to_execute_head];
            if (ROB.entry[exec_index].event_cycle <= current_core_cycle[cpu]) {
                do_execution(exec_index);

                ready_to_execute[ready_to_execute_head] = ROB_SIZE;
                ready_to_execute_head++;
                if (ready_to_execute_head == ROB_SIZE)
                    ready_to_execute_head = 0;
                exec_issued++;
            }
        }
        else {
            //DP (if (warmup_complete[cpu]) {
            //cout << "[ready_to_execute] is empty head: " << ready_to_execute_head << " tail: " << ready_to_execute_tail << endl; });
            break;
        }

        num_iteration++;
        if (num_iteration == (ROB_SIZE-1))
            break;
    }
}

void O3_CPU::do_execution(uint32_t rob_index)
{
    //if (ROB.entry[rob_index].num_reg_dependent == 0 && (ROB.entry[rob_index].scheduled == COMPLETED) && (ROB.entry[rob_index].event_cycle <= current_core_cycle[cpu])) {

  //cout << "do_execution() rob_index: " << rob_index << " cycle: " << current_core_cycle[cpu] << endl;
  
        ROB.entry[rob_index].executed = INFLIGHT;

        // ADD LATENCY
        if (warmup_complete[cpu])
        {
            if (ROB.entry[rob_index].event_cycle < current_core_cycle[cpu])
                ROB.entry[rob_index].event_cycle = current_core_cycle[cpu] + EXEC_LATENCY;
            else
                ROB.entry[rob_index].event_cycle += EXEC_LATENCY;
        }
        else
        {
            if (ROB.entry[rob_index].event_cycle < current_core_cycle[cpu])
                ROB.entry[rob_index].event_cycle = current_core_cycle[cpu];
        }


        inflight_reg_executions++;

        DP (if (warmup_complete[cpu]) {
        cout << "[ROB] " << __func__ << " non-memory instr_id: " << ROB.entry[rob_index].instr_id; 
        cout << " event_cycle: " << ROB.entry[rob_index].event_cycle << endl;});
    //}
}

uint8_t O3_CPU::mem_reg_dependence_resolved(uint32_t rob_index)
{
  return ROB.entry[rob_index].num_reg_dependent == 0;
}

void O3_CPU::schedule_memory_instruction()
{
    if ((ROB.head == ROB.tail) && ROB.occupancy == 0)
        return;

    // execution is out-of-order but we have an in-order scheduling algorithm to detect all RAW dependencies
    num_searched = 0;
    for (uint32_t i=ROB.head, count=0; count<ROB.occupancy; i=(i+1==ROB.SIZE) ? 0 : i+1, count++) {
        if ((ROB.entry[i].fetched != COMPLETED) || (ROB.entry[i].event_cycle > current_core_cycle[cpu]) || (num_searched >= SCHEDULER_SIZE))
            break;

        if (ROB.entry[i].is_memory && mem_reg_dependence_resolved(i) && (ROB.entry[i].scheduled == INFLIGHT))
            do_memory_scheduling(i);

        if (ROB.entry[i].executed == 0)
            num_searched++;
    }
}

void O3_CPU::execute_memory_instruction()
{
    operate_lsq();
    operate_cache();
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
                cout << " cannot be added in the load queue occupancy: " << LQ.occupancy << " cycle: " << current_core_cycle[cpu] << endl; });
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
                if (STA[STA_head] == ROB.entry[rob_index].instr_id) {
                    add_store_queue(rob_index, i);
                    num_added++;
                }
                //add_store_queue(rob_index, i);
                //num_added++;
            }
            else {
                DP(if(warmup_complete[cpu]) {
                cout << "[SQ] " << __func__ << " instr_id: " << ROB.entry[rob_index].instr_id;
                cout << " cannot be added in the store queue occupancy: " << SQ.occupancy << " cycle: " << current_core_cycle[cpu] << endl; });
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
    LQ.entry[lq_index].event_cycle = current_core_cycle[cpu] + SCHEDULING_LATENCY;
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
                cout << " to obey WAR store instr_id: " << SQ.entry[i].instr_id << " cycle: " << current_core_cycle[cpu] << endl; });
            }
        }
    }

    if (forwarding_index != SQ.SIZE) { // we have a store-to-load forwarding

        if ((SQ.entry[forwarding_index].fetched == COMPLETED) && (SQ.entry[forwarding_index].event_cycle <= current_core_cycle[cpu])) {
            LQ.entry[lq_index].physical_address = (SQ.entry[forwarding_index].physical_address & ~(uint64_t) ((1 << LOG2_BLOCK_SIZE) - 1)) | (LQ.entry[lq_index].virtual_address & ((1 << LOG2_BLOCK_SIZE) - 1));
            LQ.entry[lq_index].translated = COMPLETED;
            LQ.entry[lq_index].fetched = COMPLETED;

            uint32_t fwr_rob_index = LQ.entry[lq_index].rob_index;
            ROB.entry[fwr_rob_index].num_mem_ops--;
            ROB.entry[fwr_rob_index].event_cycle = current_core_cycle[cpu];
            if (ROB.entry[fwr_rob_index].num_mem_ops < 0) {
                cerr << "instr_id: " << ROB.entry[fwr_rob_index].instr_id << endl;
                assert(0);
            }
            if (ROB.entry[fwr_rob_index].num_mem_ops == 0)
                inflight_mem_executions++;

            DP(if(warmup_complete[cpu]) {
            cout << "[LQ] " << __func__ << " instr_id: " << LQ.entry[lq_index].instr_id << hex;
            cout << " full_addr: " << LQ.entry[lq_index].physical_address << dec << " is forwarded by store instr_id: ";
            cout << SQ.entry[forwarding_index].instr_id << " remain_num_ops: " << ROB.entry[fwr_rob_index].num_mem_ops << " cycle: " << current_core_cycle[cpu] << endl; });

            release_load_queue(lq_index);
        }
        else
            ; // store is not executed yet, forwarding will be handled by execute_store()
    }

    // succesfully added to the load queue
    ROB.entry[rob_index].source_added[data_index] = 1;

    if (LQ.entry[lq_index].virtual_address && (LQ.entry[lq_index].producer_id == UINT64_MAX)) { // not released and no forwarding
        RTL0[RTL0_tail] = lq_index;
        RTL0_tail++;
        if (RTL0_tail == LQ_SIZE)
            RTL0_tail = 0;

        DP (if (warmup_complete[cpu]) {
        cout << "[RTL0] " << __func__ << " instr_id: " << LQ.entry[lq_index].instr_id << " rob_index: " << LQ.entry[lq_index].rob_index << " is added to RTL0";
        cout << " head: " << RTL0_head << " tail: " << RTL0_tail << endl; }); 
    }

    DP(if(warmup_complete[cpu]) {
    cout << "[LQ] " << __func__ << " instr_id: " << LQ.entry[lq_index].instr_id;
    cout << " is added in the LQ address: " << hex << LQ.entry[lq_index].virtual_address << dec << " translated: " << +LQ.entry[lq_index].translated;
    cout << " fetched: " << +LQ.entry[lq_index].fetched << " index: " << lq_index << " occupancy: " << LQ.occupancy << " cycle: " << current_core_cycle[cpu] << endl; });
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
    SQ.entry[sq_index].event_cycle = current_core_cycle[cpu] + SCHEDULING_LATENCY;

    SQ.occupancy++;
    SQ.tail++;
    if (SQ.tail == SQ.SIZE)
        SQ.tail = 0;

    // succesfully added to the store queue
    ROB.entry[rob_index].destination_added[data_index] = 1;
    
    STA[STA_head] = UINT64_MAX;
    STA_head++;
    if (STA_head == STA_SIZE)
        STA_head = 0;

    RTS0[RTS0_tail] = sq_index;
    RTS0_tail++;
    if (RTS0_tail == SQ_SIZE)
        RTS0_tail = 0;

    DP(if(warmup_complete[cpu]) {
    cout << "[SQ] " << __func__ << " instr_id: " << SQ.entry[sq_index].instr_id;
    cout << " is added in the SQ translated: " << +SQ.entry[sq_index].translated << " fetched: " << +SQ.entry[sq_index].fetched << " is_producer: " << +ROB.entry[rob_index].is_producer;
    cout << " cycle: " << current_core_cycle[cpu] << endl; });
}

void O3_CPU::operate_lsq()
{
    // handle store
    uint32_t store_issued = 0, num_iteration = 0;

    while (store_issued < SQ_WIDTH) {
        if (RTS0[RTS0_head] < SQ_SIZE) {
            uint32_t sq_index = RTS0[RTS0_head];
            if (SQ.entry[sq_index].event_cycle <= current_core_cycle[cpu]) {

                // add it to DTLB
                PACKET data_packet;

                data_packet.fill_level = FILL_L1;
                data_packet.cpu = cpu;
                data_packet.data_index = SQ.entry[sq_index].data_index;
                data_packet.sq_index = sq_index;
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
                data_packet.event_cycle = SQ.entry[sq_index].event_cycle;
                data_packet.to_return = {&DTLB_bus};
                data_packet.sq_index_depend_on_me = {sq_index};

                DP (if (warmup_complete[cpu]) {
                cout << "[RTS0] " << __func__ << " instr_id: " << SQ.entry[sq_index].instr_id << " rob_index: " << SQ.entry[sq_index].rob_index << " is popped from to RTS0";
                cout << " head: " << RTS0_head << " tail: " << RTS0_tail << endl; }); 

                int rq_index = DTLB_bus.lower_level->add_rq(&data_packet);

                if (rq_index == -2)
                    break; 
                else 
                    SQ.entry[sq_index].translated = INFLIGHT;

                RTS0[RTS0_head] = SQ_SIZE;
                RTS0_head++;
                if (RTS0_head == SQ_SIZE)
                    RTS0_head = 0;

                store_issued++;
            }
        }
        else {
            //DP (if (warmup_complete[cpu]) {
            //cout << "[RTS0] is empty head: " << RTS0_head << " tail: " << RTS0_tail << endl; });
            break;
        }

        num_iteration++;
        if (num_iteration == (SQ_SIZE-1))
            break;
    }

    num_iteration = 0;
    while (store_issued < SQ_WIDTH) {
        if (RTS1[RTS1_head] < SQ_SIZE) {
            uint32_t sq_index = RTS1[RTS1_head];
            if (SQ.entry[sq_index].event_cycle <= current_core_cycle[cpu]) {
                execute_store(SQ.entry[sq_index].rob_index, sq_index, SQ.entry[sq_index].data_index);

                RTS1[RTS1_head] = SQ_SIZE;
                RTS1_head++;
                if (RTS1_head == SQ_SIZE)
                    RTS1_head = 0;

                store_issued++;
            }
        }
        else {
            //DP (if (warmup_complete[cpu]) {
            //cout << "[RTS1] is empty head: " << RTS1_head << " tail: " << RTS1_tail << endl; });
            break;
        }

        num_iteration++;
        if (num_iteration == (SQ_SIZE-1))
            break;
    }

    unsigned load_issued = 0;
    num_iteration = 0;
    while (load_issued < LQ_WIDTH) {
        if (RTL0[RTL0_head] < LQ_SIZE) {
            uint32_t lq_index = RTL0[RTL0_head];
            if (LQ.entry[lq_index].event_cycle <= current_core_cycle[cpu]) {

                // add it to DTLB
                PACKET data_packet;
                data_packet.fill_level = FILL_L1;
                data_packet.cpu = cpu;
                data_packet.data_index = LQ.entry[lq_index].data_index;
                data_packet.lq_index = lq_index;
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
                data_packet.event_cycle = LQ.entry[lq_index].event_cycle;
                data_packet.to_return = {&DTLB_bus};
                data_packet.lq_index_depend_on_me = {lq_index};

                DP (if (warmup_complete[cpu]) {
                cout << "[RTL0] " << __func__ << " instr_id: " << LQ.entry[lq_index].instr_id << " rob_index: " << LQ.entry[lq_index].rob_index << " is popped to RTL0";
                cout << " head: " << RTL0_head << " tail: " << RTL0_tail << endl; }); 

                int rq_index = DTLB_bus.lower_level->add_rq(&data_packet);

                if (rq_index == -2)
                    break; // break here
                else  
                    LQ.entry[lq_index].translated = INFLIGHT;

                RTL0[RTL0_head] = LQ_SIZE;
                RTL0_head++;
                if (RTL0_head == LQ_SIZE)
                    RTL0_head = 0;

                load_issued++;
            }
        }
        else {
            //DP (if (warmup_complete[cpu]) {
            //cout << "[RTL0] is empty head: " << RTL0_head << " tail: " << RTL0_tail << endl; });
            break;
        }

        num_iteration++;
        if (num_iteration == (LQ_SIZE-1))
            break;
    }

    num_iteration = 0;
    while (load_issued < LQ_WIDTH) {
        if (RTL1[RTL1_head] < LQ_SIZE) {
            uint32_t lq_index = RTL1[RTL1_head];
            if (LQ.entry[lq_index].event_cycle <= current_core_cycle[cpu]) {
                int rq_index = execute_load(LQ.entry[lq_index].rob_index, lq_index, LQ.entry[lq_index].data_index);

                if (rq_index != -2) {
                    RTL1[RTL1_head] = LQ_SIZE;
                    RTL1_head++;
                    if (RTL1_head == LQ_SIZE)
                        RTL1_head = 0;

                    load_issued++;
                }
            }
        }
        else {
            //DP (if (warmup_complete[cpu]) {
            //cout << "[RTL1] is empty head: " << RTL1_head << " tail: " << RTL1_tail << endl; });
            break;
        }

        num_iteration++;
        if (num_iteration == (LQ_SIZE-1))
            break;
    }
}

void O3_CPU::execute_store(uint32_t rob_index, uint32_t sq_index, uint32_t data_index)
{
    SQ.entry[sq_index].fetched = COMPLETED;
    SQ.entry[sq_index].event_cycle = current_core_cycle[cpu];

    ROB.entry[rob_index].num_mem_ops--;
    ROB.entry[rob_index].event_cycle = current_core_cycle[cpu];
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
	ITERATE_SET(dependent,ROB.entry[rob_index].memory_instrs_depend_on_me, ROB_SIZE) {
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
                        LQ.entry[lq_index].event_cycle = current_core_cycle[cpu];

                        uint32_t fwr_rob_index = LQ.entry[lq_index].rob_index;
                        ROB.entry[fwr_rob_index].num_mem_ops--;
                        ROB.entry[fwr_rob_index].event_cycle = current_core_cycle[cpu];
#ifdef SANITY_CHECK
                        if (ROB.entry[fwr_rob_index].num_mem_ops < 0) {
                            cerr << "instr_id: " << ROB.entry[fwr_rob_index].instr_id << endl;
                            assert(0);
                        }
#endif
                        if (ROB.entry[fwr_rob_index].num_mem_ops == 0)
                            inflight_mem_executions++;

                        DP(if(warmup_complete[cpu]) {
                        cout << "[LQ3] " << __func__ << " instr_id: " << LQ.entry[lq_index].instr_id << hex;
                        cout << " full_addr: " << LQ.entry[lq_index].physical_address << dec << " is forwarded by store instr_id: ";
                        cout << SQ.entry[sq_index].instr_id << " remain_num_ops: " << ROB.entry[fwr_rob_index].num_mem_ops << " cycle: " << current_core_cycle[cpu] << endl; });

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
    data_packet.data_index = LQ.entry[lq_index].data_index;
    data_packet.lq_index = lq_index;
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
    data_packet.event_cycle = LQ.entry[lq_index].event_cycle;
    data_packet.to_return = {&L1D_bus};
    data_packet.lq_index_depend_on_me = {lq_index};

    int rq_index = L1D_bus.lower_level->add_rq(&data_packet);

    if (rq_index == -2)
        return rq_index;
    else 
        LQ.entry[lq_index].fetched = INFLIGHT;

    return rq_index;
}

void O3_CPU::do_complete_execution(uint32_t rob_index)
{
    ROB.entry[rob_index].executed = COMPLETED;
    if (ROB.entry[rob_index].is_memory == 0)
        inflight_reg_executions--;
    else
        inflight_mem_executions--;

    completed_executions++;

    for (auto dependent : ROB.entry[rob_index].registers_instrs_depend_on_me)
    {
        dependent->num_reg_dependent--;

        if (dependent->num_reg_dependent == 0) {
            if (dependent->is_memory)
                dependent->scheduled = INFLIGHT;
            else {
                dependent->scheduled = COMPLETED;
            }
        }
    }

    if (ROB.entry[rob_index].branch_mispredicted)
        fetch_resume_cycle = current_core_cycle[cpu] + BRANCH_MISPREDICT_PENALTY;
}

void O3_CPU::operate_cache()
{
    L2C.operate();
    L1D.operate();
    L1I.operate();
    STLB.operate();
    DTLB.operate();
    ITLB.operate();

    // also handle per-cycle prefetcher operation
    l1i_prefetcher_cycle_operate();
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

        if ((ROB.entry[i].executed == INFLIGHT) && (ROB.entry[i].event_cycle <= current_core_cycle[cpu]) && ROB.entry[i].num_mem_ops == 0)
        {
            do_complete_execution(i);
            ++instrs_executed;

            auto begin_dep = std::begin(ROB.entry[i].registers_instrs_depend_on_me);
            auto end_dep   = std::end(ROB.entry[i].registers_instrs_depend_on_me);
            std::sort(begin_dep, end_dep);
            auto last = std::unique(begin_dep, end_dep);
            ROB.entry[i].registers_instrs_depend_on_me.erase(last, end_dep);
            for (auto dependent : ROB.entry[i].registers_instrs_depend_on_me)
            {
                if (dependent->scheduled == COMPLETED && dependent->num_reg_dependent == 0)
                {
                    assert(ready_to_execute[ready_to_execute_tail] == ROB_SIZE);
                    ready_to_execute[ready_to_execute_tail] = std::distance(ROB.entry, dependent);

                    DP (if (warmup_complete[cpu]) {
                            cout << "[ready_to_execute] " << __func__ << " instr_id: " << dependent->instr_id << " rob_index: " << rob_index << " is added to ready_to_execute";
                            cout << " head: " << ready_to_execute_head << " tail: " << ready_to_execute_tail << endl; }); 

                    ready_to_execute_tail++;
                    if (ready_to_execute_tail == ROB_SIZE)
                        ready_to_execute_tail = 0;
                }
            }
        }
	}
    }
}

void O3_CPU::handle_memory_return()
{
  // Instruction Memory

  std::size_t available_fetch_bandwidth = FETCH_WIDTH;
  std::size_t to_read = static_cast<CACHE*>(ITLB_bus.lower_level)->MAX_READ;

  while (available_fetch_bandwidth > 0 && to_read > 0 && !ITLB_bus.PROCESSED.empty() && ITLB_bus.PROCESSED.front().event_cycle <= current_core_cycle[cpu])
  {
      PACKET &itlb_entry = ITLB_bus.PROCESSED.front();

      // mark the appropriate instructions in the IFETCH_BUFFER as translated and ready to fetch
      while (!itlb_entry.instr_depend_on_me.empty())
      {
          auto it = itlb_entry.instr_depend_on_me.front();
          if (available_fetch_bandwidth > 0)
          {
              if ((it->ip >> LOG2_PAGE_SIZE) == (itlb_entry.address) && it->translated != 0)
              {
                  it->translated = COMPLETED;
                  // recalculate a physical address for this cache line based on the translated physical page address
                  it->instruction_pa = (itlb_entry.data << LOG2_PAGE_SIZE) | (it->ip & ((1 << LOG2_PAGE_SIZE) - 1));

                  available_fetch_bandwidth--;
              }

              itlb_entry.instr_depend_on_me.pop_front();
          }
          else
          {
              // not enough fetch bandwidth to translate this instruction this time, so try again next cycle
              break;
          }
      }

      // remove this entry if we have serviced all of its instructions
      if (itlb_entry.instr_depend_on_me.empty())
          ITLB_bus.PROCESSED.pop_front();
      --to_read;
  }

  available_fetch_bandwidth = FETCH_WIDTH;
  to_read = static_cast<CACHE*>(L1I_bus.lower_level)->MAX_READ;

  while (available_fetch_bandwidth > 0 && to_read > 0 && !L1I_bus.PROCESSED.empty() && L1I_bus.PROCESSED.front().event_cycle <= current_core_cycle[cpu])
  {
      PACKET &l1i_entry = L1I_bus.PROCESSED.front();

      // this is the L1I cache, so instructions are now fully fetched, so mark them as such
      while (!l1i_entry.instr_depend_on_me.empty())
      {
          auto it = l1i_entry.instr_depend_on_me.front();
          if (available_fetch_bandwidth > 0)
          {
             if ((it->instruction_pa >> LOG2_BLOCK_SIZE) == (l1i_entry.address) && it->fetched != 0 && it->translated == COMPLETED)
             {
                 it->fetched = COMPLETED;
                 available_fetch_bandwidth--;
             }

             l1i_entry.instr_depend_on_me.pop_front();
          }
          else
          {
              // not enough fetch bandwidth to mark instructions from this block this time, so try again next cycle
              break;
          }
      }

      // remove this entry if we have serviced all of its instructions
      if (l1i_entry.instr_depend_on_me.empty())
          L1I_bus.PROCESSED.pop_front();
      --to_read;
  }

  // Data Memory
  to_read = static_cast<CACHE*>(DTLB_bus.lower_level)->MAX_READ;

  while (to_read > 0 && !DTLB_bus.PROCESSED.empty() && (DTLB_bus.PROCESSED.front().event_cycle <= current_core_cycle[cpu]))
	{ // DTLB
	  PACKET &dtlb_entry = DTLB_bus.PROCESSED.front();

	  for (auto sq_merged : dtlb_entry.sq_index_depend_on_me)
	    {
	      SQ.entry[sq_merged].physical_address = (dtlb_entry.data << LOG2_PAGE_SIZE) | (SQ.entry[sq_merged].virtual_address & ((1 << LOG2_PAGE_SIZE) - 1)); // translated address
	      SQ.entry[sq_merged].translated = COMPLETED;
	      SQ.entry[sq_merged].event_cycle = current_core_cycle[cpu];

	      RTS1[RTS1_tail] = sq_merged;
	      RTS1_tail++;
	      if (RTS1_tail == SQ_SIZE)
		RTS1_tail = 0;
	    }

	  for (auto lq_merged : dtlb_entry.lq_index_depend_on_me)
	    {
	      LQ.entry[lq_merged].physical_address = (dtlb_entry.data << LOG2_PAGE_SIZE) | (LQ.entry[lq_merged].virtual_address & ((1 << LOG2_PAGE_SIZE) - 1)); // translated address
	      LQ.entry[lq_merged].translated = COMPLETED;
	      LQ.entry[lq_merged].event_cycle = current_core_cycle[cpu];

	      RTL1[RTL1_tail] = lq_merged;
	      RTL1_tail++;
	      if (RTL1_tail == LQ_SIZE)
		RTL1_tail = 0;
	    }

	  ROB.entry[dtlb_entry.rob_index].event_cycle = dtlb_entry.event_cycle;

	  // remove this entry
	  DTLB_bus.PROCESSED.pop_front();
      --to_read;
    }

  to_read = static_cast<CACHE*>(L1D_bus.lower_level)->MAX_READ;
  while (to_read > 0 && !L1D_bus.PROCESSED.empty() && (L1D_bus.PROCESSED.front().event_cycle <= current_core_cycle[cpu]))
	{ // L1D
	  PACKET &l1d_entry = L1D_bus.PROCESSED.front();

	  for (auto merged : l1d_entry.lq_index_depend_on_me)
	    {
	      LQ.entry[merged].fetched = COMPLETED;
	      LQ.entry[merged].event_cycle = current_core_cycle[cpu];
	      ROB.entry[LQ.entry[merged].rob_index].num_mem_ops--;
	      ROB.entry[LQ.entry[merged].rob_index].event_cycle = l1d_entry.event_cycle;

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
  if ((ROB.entry[ROB.head].executed != COMPLETED) || (ROB.entry[ROB.head].event_cycle > current_core_cycle[cpu]))
    {
      return;
    }

    for (uint32_t n=0; n<RETIRE_WIDTH; n++) {
        if (ROB.entry[ROB.head].ip == 0)
            return;

        // retire is in-order
        if (ROB.entry[ROB.head].executed != COMPLETED) { 
            DP ( if (warmup_complete[cpu]) {
            cout << "[ROB] " << __func__ << " instr_id: " << ROB.entry[ROB.head].instr_id << " head: " << ROB.head << " is not executed yet" << endl; });
            return;
        }

        // check store instruction
        uint32_t num_store = 0;
        for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
            if (ROB.entry[ROB.head].destination_memory[i])
                num_store++;
        }

        if (num_store) {
                for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
                    if (ROB.entry[ROB.head].destination_memory[i]) {

                        PACKET data_packet;
                        uint32_t sq_index = ROB.entry[ROB.head].sq_index[i];

                        // sq_index and rob_index are no longer available after retirement
                        // but we pass this information to avoid segmentation fault
                        data_packet.fill_level = FILL_L1;
                        data_packet.cpu = cpu;
                        data_packet.data_index = SQ.entry[sq_index].data_index;
                        data_packet.sq_index = sq_index;
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
                        data_packet.event_cycle = current_core_cycle[cpu];

                        auto result = L1D_bus.lower_level->add_wq(&data_packet);
                        if (result != -2)
                            ROB.entry[ROB.head].destination_memory[i] = 0;
                        else
                            return;
                }
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
    }
}

void CacheBus::return_data(PACKET *packet)
{
    if (packet->type != PREFETCH)
    {
        //std::cout << "add to processed" << std::endl;
        PROCESSED.push_back(*packet);
    }
}

