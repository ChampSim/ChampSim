#include "ooo_cpu.h"
#include "set.h"

#include <array>
#include <iostream>

// out-of-order core
std::array<O3_CPU, NUM_CPUS> ooo_cpu;
std::array<uint64_t, NUM_CPUS> current_core_cycle, stall_cycle;
uint32_t SCHEDULING_LATENCY = 0, EXEC_LATENCY = 0, DECODE_LATENCY = 0;

void O3_CPU::initialize_core()
{

}

void O3_CPU::read_from_trace()
{
    // actual processors do not work like this but for easier implementation,
    // we read instruction traces and virtually add them in the ROB
    // note that these traces are not yet translated and fetched 

    uint8_t continue_reading = 1;
    uint32_t num_reads = 0;
    instrs_to_read_this_cycle = FETCH_WIDTH;

    // first, read PIN trace
    while (continue_reading) {

        size_t instr_size = knob_cloudsuite ? sizeof(cloudsuite_instr) : sizeof(input_instr);

        if (knob_cloudsuite) {
            if (!fread(&current_cloudsuite_instr, instr_size, 1, trace_file)) {
                // reached end of file for this trace
                cout << "*** Reached end of trace for Core: " << cpu << " Repeating trace: " << trace_string << endl; 

                // close the trace file and re-open it
                pclose(trace_file);
                trace_file = popen(gunzip_command, "r");
                if (trace_file == NULL) {
                    cerr << endl << "*** CANNOT REOPEN TRACE FILE: " << trace_string << " ***" << endl;
                    assert(0);
                }
            } else { // successfully read the trace

                // copy the instruction into the performance model's instruction format
                ooo_model_instr arch_instr;
                int num_reg_ops = 0, num_mem_ops = 0;

                arch_instr.instr_id = instr_unique_id;
                arch_instr.ip = current_cloudsuite_instr.ip;
                arch_instr.is_branch = current_cloudsuite_instr.is_branch;
                arch_instr.branch_taken = current_cloudsuite_instr.branch_taken;

                arch_instr.asid[0] = current_cloudsuite_instr.asid[0];
                arch_instr.asid[1] = current_cloudsuite_instr.asid[1];

                for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
                    arch_instr.destination_registers[i] = current_cloudsuite_instr.destination_registers[i];
                    arch_instr.destination_memory[i] = current_cloudsuite_instr.destination_memory[i];
                    arch_instr.destination_virtual_address[i] = current_cloudsuite_instr.destination_memory[i];

                    if (arch_instr.destination_registers[i])
                        num_reg_ops++;
                    if (arch_instr.destination_memory[i]) {
                        num_mem_ops++;

                        // update STA, this structure is required to execute store instructions properly without deadlock
                        if (num_mem_ops > 0) {
                            while (STA.size() >= STA_SIZE)
                                STA.pop();
                            STA.push(instr_unique_id);
                        }
                    }
                }

                for (int i=0; i<NUM_INSTR_SOURCES; i++) {
                    arch_instr.source_registers[i] = current_cloudsuite_instr.source_registers[i];
                    arch_instr.source_memory[i] = current_cloudsuite_instr.source_memory[i];
                    arch_instr.source_virtual_address[i] = current_cloudsuite_instr.source_memory[i];

                    if (arch_instr.source_registers[i])
                        num_reg_ops++;
                    if (arch_instr.source_memory[i])
                        num_mem_ops++;
                }

                arch_instr.num_reg_ops = num_reg_ops;
                arch_instr.num_mem_ops = num_mem_ops;
                if (num_mem_ops > 0) 
                    arch_instr.is_memory = 1;

                // add this instruction to the IFETCH_BUFFER
                if (IFETCH_BUFFER.occupancy < IFETCH_BUFFER.SIZE) {
		  uint32_t ifetch_buffer_index = add_to_ifetch_buffer(&arch_instr);
          ooo_model_instr &ifb_entry = IFETCH_BUFFER.entry.at(ifetch_buffer_index);
		  num_reads++;

		  // handle branch prediction
          if (ifb_entry.is_branch)
          {
		    DP( if (warmup_complete[cpu]) {
                        cout << "[BRANCH] instr_id: " << instr_unique_id << " ip: " << hex << arch_instr.ip << dec << " taken: " << +arch_instr.branch_taken << endl; });
		    
		    num_branch++;
		    
		    // handle branch prediction & branch predictor update
            uint8_t branch_prediction = predict_branch(ifb_entry.ip);
		    
            if(ifb_entry.branch_taken != branch_prediction)
		      {
			branch_mispredictions++;
			total_rob_occupancy_at_branch_mispredict += ROB.occupancy;
			if(warmup_complete[cpu])
			  {
			    fetch_stall = 1;
			    instrs_to_read_this_cycle = 0;
                ifb_entry.branch_mispredicted = 1;
			  }
		      }
		    else
		      {
			// correct prediction
			if(branch_prediction == 1)
			  {
			    // if correctly predicted taken, then we can't fetch anymore instructions this cycle
			    instrs_to_read_this_cycle = 0;
			  }
		      }
		    
            last_branch_result(ifb_entry.ip, ifb_entry.branch_taken);
		  }
		  
		  if ((num_reads >= instrs_to_read_this_cycle) || (IFETCH_BUFFER.occupancy == IFETCH_BUFFER.SIZE))
		    continue_reading = 0;
                }
                instr_unique_id++;
            }
        }
	else
	  {
	    input_instr trace_read_instr;
            if (!fread(&trace_read_instr, instr_size, 1, trace_file))
	      {
                // reached end of file for this trace
                cout << "*** Reached end of trace for Core: " << cpu << " Repeating trace: " << trace_string << endl; 
		
                // close the trace file and re-open it
                pclose(trace_file);
                trace_file = popen(gunzip_command, "r");
                if (trace_file == NULL) {
		  cerr << endl << "*** CANNOT REOPEN TRACE FILE: " << trace_string << " ***" << endl;
                    assert(0);
                }
            }
	    else
	      { // successfully read the trace

		if(instr_unique_id == 0)
		  {
		    current_instr = next_instr = trace_read_instr;
		  }
		else
		  {
		    current_instr = next_instr;
		    next_instr = trace_read_instr;
		  }

                // copy the instruction into the performance model's instruction format
                ooo_model_instr arch_instr;
                int num_reg_ops = 0, num_mem_ops = 0;

                arch_instr.instr_id = instr_unique_id;
                arch_instr.ip = current_instr.ip;
                arch_instr.is_branch = current_instr.is_branch;
                arch_instr.branch_taken = current_instr.branch_taken;

                arch_instr.asid[0] = cpu;
                arch_instr.asid[1] = cpu;

		bool reads_sp = false;
		bool writes_sp = false;
		bool reads_flags = false;
		bool reads_ip = false;
		bool writes_ip = false;
		bool reads_other = false;

                for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
                    arch_instr.destination_registers[i] = current_instr.destination_registers[i];
                    arch_instr.destination_memory[i] = current_instr.destination_memory[i];
                    arch_instr.destination_virtual_address[i] = current_instr.destination_memory[i];

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
                        num_reg_ops++;
                    if (arch_instr.destination_memory[i]) {
                        num_mem_ops++;

                        // update STA, this structure is required to execute store instructions properly without deadlock
                        if (num_mem_ops > 0) {
                            while (STA.size() >= STA_SIZE)
                                STA.pop();
                            STA.push(instr_unique_id);
                        }
                    }
                }

                for (int i=0; i<NUM_INSTR_SOURCES; i++) {
                    arch_instr.source_registers[i] = current_instr.source_registers[i];
                    arch_instr.source_memory[i] = current_instr.source_memory[i];
                    arch_instr.source_virtual_address[i] = current_instr.source_memory[i];

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
                        num_reg_ops++;
                    if (arch_instr.source_memory[i])
                        num_mem_ops++;
                }

                arch_instr.num_reg_ops = num_reg_ops;
                arch_instr.num_mem_ops = num_mem_ops;
                if (num_mem_ops > 0) 
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
		
		if((arch_instr.is_branch == 1) && (arch_instr.branch_taken == 1))
		  {
		    arch_instr.branch_target = next_instr.ip;
		  }

                // add this instruction to the IFETCH_BUFFER
                if (IFETCH_BUFFER.occupancy < IFETCH_BUFFER.SIZE) {
		  uint32_t ifetch_buffer_index = add_to_ifetch_buffer(&arch_instr);
          ooo_model_instr &ifb_entry = IFETCH_BUFFER.entry.at(ifetch_buffer_index);
		  num_reads++;

                    // handle branch prediction
                    if (ifb_entry.is_branch) {

                        DP( if (warmup_complete[cpu]) {
                        cout << "[BRANCH] instr_id: " << instr_unique_id << " ip: " << hex << arch_instr.ip << dec << " taken: " << +arch_instr.branch_taken << endl; });

                        num_branch++;

			// handle branch prediction & branch predictor update
                        uint8_t branch_prediction = predict_branch(ifb_entry.ip);
                        uint64_t predicted_branch_target = ifb_entry.branch_target;
			if(branch_prediction == 0)
			  {
			    predicted_branch_target = 0;
			  }
			// call code prefetcher every time the branch predictor is used
            l1i_prefetcher_branch_operate(ifb_entry.ip, ifb_entry.branch_type, predicted_branch_target);

            if(ifb_entry.branch_taken != branch_prediction)
			  {
			    branch_mispredictions++;
			    total_rob_occupancy_at_branch_mispredict += ROB.occupancy;
			    if(warmup_complete[cpu])
			      {
				fetch_stall = 1;
				instrs_to_read_this_cycle = 0;
                ifb_entry.branch_mispredicted = 1;
			      }
			  }
			else
			  {
			    // correct prediction
			    if(branch_prediction == 1)
			      {
				// if correctly predicted taken, then we can't fetch anymore instructions this cycle
				instrs_to_read_this_cycle = 0;
			      }
			  }
			
            last_branch_result(ifb_entry.ip, ifb_entry.branch_taken);
                    }

                    if ((num_reads >= instrs_to_read_this_cycle) || (IFETCH_BUFFER.occupancy == IFETCH_BUFFER.SIZE))
                        continue_reading = 0;
                }
                instr_unique_id++;
            }
        }
    }

    //instrs_to_fetch_this_cycle = num_reads;
}

uint32_t O3_CPU::add_to_rob(ooo_model_instr *arch_instr)
{
    ooo_model_instr &rob_entry = ROB.entry.at(ROB.tail);

    // sanity check
    if (rob_entry.instr_id != 0) {
        std::cerr << "[ROB_ERROR] " << __func__ << " is not empty index: " << ROB.tail;
        std::cerr << " instr_id: " << rob_entry.instr_id << std::endl;
        assert(0);
    }

    rob_entry = *arch_instr;
    rob_entry.event_cycle = current_core_cycle[cpu];

    ROB.occupancy++;
    ROB.tail++;
    if (ROB.tail >= ROB.SIZE)
        ROB.tail = 0;

    DP ( if (warmup_complete[cpu]) {
            std::cout << "[ROB] " <<  __func__ << " instr_id: " << rob_entry.instr_id;
            std::cout << " ip: " << std::hex << rob_entry.ip << std::dec;
            std::cout << " head: " << ROB.head << " tail: " << ROB.tail << " occupancy: " << ROB.occupancy;
            std::cout << " event: " << rob_entry.event_cycle << " current: " << current_core_cycle[cpu] << std::endl; });

#ifdef SANITY_CHECK
    if (rob_entry.ip == 0)
    {
        std::cerr << "[ROB_ERROR] " << __func__ << " ip is zero";
        std::cerr << " instr_id: " << rob_entry.instr_id << " ip: " << rob_entry.ip << std::endl;
        assert(0);
    }
#endif

    return ROB.tail == 0 ? ROB.SIZE-1 : ROB.tail-1;
}

uint32_t O3_CPU::add_to_ifetch_buffer(ooo_model_instr *arch_instr)
{
  /*
  if((arch_instr->is_branch != 0) && (arch_instr->branch_type == BRANCH_OTHER))
    {
      cout << "IP: 0x" << hex << (uint64_t)(arch_instr->ip) << " branch_target: 0x" << (uint64_t)(arch_instr->branch_target) << dec << endl;
      cout << (uint32_t)(arch_instr->is_branch) << " " << (uint32_t)(arch_instr->branch_type) << " " << (uint32_t)(arch_instr->branch_taken) << endl;
      for(uint32_t i=0; i<NUM_INSTR_SOURCES; i++)
	{
	  cout << (uint32_t)(arch_instr->source_registers[i]) << " ";
	}
      cout << endl;
      for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++)
	{
	  cout << (uint32_t)(arch_instr->destination_registers[i]) << " ";
	}
      cout << endl << endl;
    }
  */

    ooo_model_instr &ifb_entry = IFETCH_BUFFER.entry.at(IFETCH_BUFFER.tail);

    if(ifb_entry.instr_id != 0)
    {
        std::cerr << "[IFETCH_BUFFER_ERROR] " << __func__ << " is not empty index: " << IFETCH_BUFFER.tail;
        std::cerr << " instr_id: " << ifb_entry.instr_id << std::endl;
      assert(0);
    }

    ifb_entry = *arch_instr;
    ifb_entry.event_cycle = current_core_cycle[cpu];

  // magically translate instructions
    uint64_t instr_pa = va_to_pa(cpu, ifb_entry.instr_id, ifb_entry.ip , (ifb_entry.ip)>>LOG2_PAGE_SIZE, 1);
    instr_pa >>= LOG2_PAGE_SIZE;
    instr_pa <<= LOG2_PAGE_SIZE;
    instr_pa |= (ifb_entry.ip & ((1 << LOG2_PAGE_SIZE) - 1));  
    ifb_entry.instruction_pa = instr_pa;
    ifb_entry.translated = COMPLETED;
    ifb_entry.fetched = 0;
  // end magic

    IFETCH_BUFFER.occupancy++;
    IFETCH_BUFFER.tail++;

    if(IFETCH_BUFFER.tail >= IFETCH_BUFFER.SIZE)
    {
      IFETCH_BUFFER.tail = 0;
    }

    return IFETCH_BUFFER.tail == 0 ? IFETCH_BUFFER.SIZE-1 : IFETCH_BUFFER.tail-1;
}

uint32_t O3_CPU::add_to_decode_buffer(ooo_model_instr *arch_instr)
{
    ooo_model_instr &db_entry = DECODE_BUFFER.entry.at(DECODE_BUFFER.tail);

    if(db_entry.instr_id != 0)
    {
        std::cerr << "[DECODE_BUFFER_ERROR] " << __func__ << " is not empty index: " << DECODE_BUFFER.tail;
        std::cerr << " instr_id: " << db_entry.instr_id << std::endl;
      assert(0);
    }

    db_entry = *arch_instr;
    db_entry.event_cycle = current_core_cycle[cpu];

    DECODE_BUFFER.occupancy++;
    DECODE_BUFFER.tail++;
    if(DECODE_BUFFER.tail >= DECODE_BUFFER.SIZE)
    {
      DECODE_BUFFER.tail = 0;
    }

    return DECODE_BUFFER.tail == 0 ? DECODE_BUFFER.SIZE-1 : DECODE_BUFFER.tail-1;
}

uint32_t O3_CPU::check_rob(uint64_t instr_id)
{
    if ((ROB.head == ROB.tail) && ROB.occupancy == 0)
        return ROB.SIZE;

    if (ROB.head < ROB.tail) {
        for (uint32_t i=ROB.head; i<ROB.tail; i++) {
            if (ROB.entry.at(i).instr_id == instr_id) {
                DP ( if (warmup_complete[ROB.cpu]) {
                cout << "[ROB] " << __func__ << " same instr_id: " << ROB.entry.at(i).instr_id;
                cout << " rob_index: " << i << endl; });
                return i;
            }
        }
    }
    else {
        for (uint32_t i=ROB.head; i<ROB.SIZE; i++) {
            if (ROB.entry.at(i).instr_id == instr_id) {
                DP ( if (warmup_complete[cpu]) {
                cout << "[ROB] " << __func__ << " same instr_id: " << ROB.entry.at(i).instr_id;
                cout << " rob_index: " << i << endl; });
                return i;
            }
        }
        for (uint32_t i=0; i<ROB.tail; i++) {
            if (ROB.entry.at(i).instr_id == instr_id) {
                DP ( if (warmup_complete[cpu]) {
                cout << "[ROB] " << __func__ << " same instr_id: " << ROB.entry.at(i).instr_id;
                cout << " rob_index: " << i << endl; });
                return i;
            }
        }
    }

    cerr << "[ROB_ERROR] " << __func__ << " does not have any matching index! ";
    cerr << " instr_id: " << instr_id << endl;
    assert(0);

    return ROB.SIZE;
}

void O3_CPU::fetch_instruction()
{
  // TODO: can we model wrong path execusion?
  // probalby not
  
  // if we had a branch mispredict, turn fetching back on after the branch mispredict penalty
  if((fetch_stall == 1) && (current_core_cycle[cpu] >= fetch_resume_cycle) && (fetch_resume_cycle != 0))
    {
      fetch_stall = 0;
      fetch_resume_cycle = 0;
    }

  if(IFETCH_BUFFER.occupancy == 0)
    {
      return;
    }

  // scan through IFETCH_BUFFER to find instructions that need to be translated
  uint32_t index = IFETCH_BUFFER.head;
  for(uint32_t i=0; i<IFETCH_BUFFER.SIZE; i++)
    {
        ooo_model_instr &ifb_entry = IFETCH_BUFFER.entry.at(index);
      if(ifb_entry.ip == 0)
	{
	  break;
	}

      if(IFETCH_BUFFER.entry.at(index).translated == 0)
	{
	  // begin process of fetching this instruction by sending it to the ITLB
	  // add it to the ITLB's read queue
	  PACKET trace_packet;
	  trace_packet.instruction = 1;
	  trace_packet.is_data = 0;
	  trace_packet.tlb_access = 1;
	  trace_packet.fill_level = FILL_L1;
	  trace_packet.fill_l1i = 1;
	  trace_packet.cpu = cpu;
      trace_packet.address = ifb_entry.ip >> LOG2_PAGE_SIZE;
	  if (knob_cloudsuite)
          trace_packet.address = ifb_entry.ip >> LOG2_PAGE_SIZE;
	  else
          trace_packet.address = ifb_entry.ip >> LOG2_PAGE_SIZE;
      trace_packet.full_addr = ifb_entry.ip;
	  trace_packet.instr_id = 0;
	  trace_packet.rob_index = i;
	  trace_packet.producer = 0; // TODO: check if this guy gets used or not
      trace_packet.ip = ifb_entry.ip;
	  trace_packet.type = LOAD; 
	  trace_packet.asid[0] = 0;
	  trace_packet.asid[1] = 0;
	  trace_packet.event_cycle = current_core_cycle[cpu];
	  
	  int rq_index = ITLB.add_rq(&trace_packet);

	  if(rq_index != -2)
	    {
	      // successfully sent to the ITLB, so mark all instructions in the IFETCH_BUFFER that match this ip as translated INFLIGHT
	      for(uint32_t j=0; j<IFETCH_BUFFER.SIZE; j++)
		{
            if(((IFETCH_BUFFER.entry.at(j).ip>>LOG2_PAGE_SIZE) == (ifb_entry.ip>>LOG2_PAGE_SIZE)) && (IFETCH_BUFFER.entry.at(j).translated == 0))
		    {
		      IFETCH_BUFFER.entry.at(j).translated = INFLIGHT;
		      IFETCH_BUFFER.entry.at(j).fetched = 0;
		    }
		}
	    }
	}

      // fetch cache lines that were part of a translated page but not the cache line that initiated the translation
      if((ifb_entry.translated == COMPLETED) && (ifb_entry.fetched == 0))
	{
	  // add it to the L1-I's read queue
	  PACKET fetch_packet;
	  fetch_packet.instruction = 1;
	  fetch_packet.is_data = 0;
	  fetch_packet.fill_level = FILL_L1;
	  fetch_packet.fill_l1i = 1;
	  fetch_packet.cpu = cpu;
      fetch_packet.address = ifb_entry.instruction_pa >> 6;
      fetch_packet.instruction_pa = ifb_entry.instruction_pa;
      fetch_packet.full_addr = ifb_entry.instruction_pa;
	  fetch_packet.instr_id = 0;
	  fetch_packet.rob_index = 0;
	  fetch_packet.producer = 0;
      fetch_packet.ip = ifb_entry.ip;
	  fetch_packet.type = LOAD; 
	  fetch_packet.asid[0] = 0;
	  fetch_packet.asid[1] = 0;
	  fetch_packet.event_cycle = current_core_cycle[cpu];

	  /*
	  // invoke code prefetcher -- THIS HAS BEEN MOVED TO cache.cc !!!
	  int hit_way = L1I.check_hit(&fetch_packet);
	  uint8_t prefetch_hit = 0;
	  if(hit_way != -1)
	    {
	      prefetch_hit = L1I.block[L1I.get_set(fetch_packet.address)][hit_way].prefetch;
	    }
	  l1i_prefetcher_cache_operate(fetch_packet.ip, (hit_way != -1), prefetch_hit);
	  */
	  
	  int rq_index = L1I.add_rq(&fetch_packet);

	  if(rq_index != -2)
	    {
	      // mark all instructions from this cache line as having been fetched
	      for(uint32_t j=0; j<IFETCH_BUFFER.SIZE; j++)
		{
            if(IFETCH_BUFFER.entry.at(j).ip>>6 == ifb_entry.ip>>6)
		    {
		      IFETCH_BUFFER.entry.at(j).translated = COMPLETED;
		      IFETCH_BUFFER.entry.at(j).fetched = INFLIGHT;
		    }
		}
	    }
	}

      index++;
      if(index >= IFETCH_BUFFER.SIZE)
	{
	  index = 0;
	}
      
      if(index == IFETCH_BUFFER.head)
	{
	  break;
	}
    }
  
  // send to DECODE stage
  bool decode_full = false;
  for(uint32_t i=0; i<DECODE_WIDTH; i++)
    {
      if(decode_full)
	{
          break;
        }

      ooo_model_instr &ifb_entry = IFETCH_BUFFER.entry.at(IFETCH_BUFFER.head);

      if(ifb_entry.ip == 0)
        {
          break;
	}	      
      
      if(ifb_entry.translated == COMPLETED && ifb_entry.fetched == COMPLETED)
	{
	  if(DECODE_BUFFER.occupancy < DECODE_BUFFER.SIZE)
	    {
            uint32_t decode_index = add_to_decode_buffer(&IFETCH_BUFFER.entry[IFETCH_BUFFER.head]);
	      DECODE_BUFFER.entry.at(decode_index).event_cycle = 0;
	      
	      ooo_model_instr empty_entry;
          ifb_entry = empty_entry;
	      
	      IFETCH_BUFFER.head++;
	      if(IFETCH_BUFFER.head >= IFETCH_BUFFER.SIZE)
		{
		  IFETCH_BUFFER.head = 0;
		}
	      IFETCH_BUFFER.occupancy--;
	    }
	  else
	    {
	      decode_full = true;
	    }
	}

      index++;
      if(index >= IFETCH_BUFFER.SIZE)
        {
          index = 0;
	}
    }
}

void O3_CPU::decode_and_dispatch()
{
    // dispatch DECODE_WIDTH instructions that have decoded into the ROB
    uint32_t count_dispatches = 0;
    for(uint32_t i=0; i<DECODE_BUFFER.SIZE; i++)
    {
        ooo_model_instr &db_entry = DECODE_BUFFER.entry.at(DECODE_BUFFER.head);
        if(db_entry.ip == 0)
        {
            break;
        }

        if(ROB.occupancy < ROB.SIZE && (!warmup_complete[cpu] || (db_entry.event_cycle != 0 && db_entry.event_cycle < current_core_cycle[cpu])))
        {
            // move this instruction to the ROB if there's space
            uint32_t rob_index = add_to_rob(&DECODE_BUFFER.entry.at(DECODE_BUFFER.head));
            //std::cout << rob_index << std::endl;
            ROB.entry.at(rob_index).event_cycle = current_core_cycle[cpu];

            ooo_model_instr empty_entry;
            db_entry = empty_entry;

            DECODE_BUFFER.head++;
            if(DECODE_BUFFER.head >= DECODE_BUFFER.SIZE)
            {
                DECODE_BUFFER.head = 0;
            }
            DECODE_BUFFER.occupancy--;

            count_dispatches++;
            if(count_dispatches >= DECODE_WIDTH)
            {
                break;
            }
        }
        else
        {
            break;
        }
    }

    // make new instructions pay decode penalty if they miss in the decoded instruction cache
    uint32_t decode_index = DECODE_BUFFER.head;
    uint32_t count_decodes = 0;
    for(uint32_t i=0; i<DECODE_BUFFER.SIZE; i++)
    {
        ooo_model_instr &db_entry = DECODE_BUFFER.entry.at(decode_index);
        if(DECODE_BUFFER.entry[DECODE_BUFFER.head].ip == 0)
        {
            break;
        }

        //std::cout << DECODE_BUFFER.head << "  " << decode_index << std::endl;

        if(db_entry.event_cycle == 0)
        {
            // apply decode latency
            db_entry.event_cycle = current_core_cycle[cpu] + DECODE_LATENCY;
        }

        if(decode_index == DECODE_BUFFER.tail)
        {
            break;
        }
        decode_index++;
        if(decode_index >= DECODE_BUFFER.SIZE)
        {
            decode_index = 0;
        }

        count_decodes++;
        if(count_decodes > DECODE_WIDTH)
        {
            break;
        }
    }
}

int O3_CPU::prefetch_code_line(uint64_t pf_v_addr)
{
  if(pf_v_addr == 0)
    {
      cerr << "Cannot prefetch code line 0x0 !!!" << endl;
      assert(0);
    }
  
  L1I.pf_requested++;

  if (L1I.PQ.occupancy < L1I.PQ.SIZE)
    {
      // magically translate prefetches
      uint64_t pf_pa = (va_to_pa(cpu, 0, pf_v_addr, pf_v_addr>>LOG2_PAGE_SIZE, 1) & (~((1 << LOG2_PAGE_SIZE) - 1))) | (pf_v_addr & ((1 << LOG2_PAGE_SIZE) - 1));

      PACKET pf_packet;
      pf_packet.instruction = 1; // this is a code prefetch
      pf_packet.is_data = 0;
      pf_packet.fill_level = FILL_L1;
      pf_packet.fill_l1i = 1;
      pf_packet.pf_origin_level = FILL_L1;
      pf_packet.cpu = cpu;

      pf_packet.address = pf_pa >> LOG2_BLOCK_SIZE;
      pf_packet.full_addr = pf_pa;

      pf_packet.ip = pf_v_addr;
      pf_packet.type = PREFETCH;
      pf_packet.event_cycle = current_core_cycle[cpu];

      L1I.add_pq(&pf_packet);    
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

    // execution is out-of-order but we have an in-order scheduling algorithm to detect all RAW dependencies
    uint32_t limit = ROB.next_fetch[1];
    num_searched = 0;
    if (ROB.head < limit) {
        for (uint32_t i=ROB.head; i<limit; i++) { 
            ooo_model_instr &rob_entry = ROB.entry.at(i);
            if ((rob_entry.fetched != COMPLETED) || (rob_entry.event_cycle > current_core_cycle[cpu]) || (num_searched >= SCHEDULER_SIZE))
                return;

            if (rob_entry.scheduled == 0)
                do_scheduling(i);

            num_searched++;
        }
    }
    else {
        for (uint32_t i=ROB.head; i<ROB.SIZE; i++) {
            ooo_model_instr &rob_entry = ROB.entry.at(i);
            if ((rob_entry.fetched != COMPLETED) || (rob_entry.event_cycle > current_core_cycle[cpu]) || (num_searched >= SCHEDULER_SIZE))
                return;

            if (rob_entry.scheduled == 0)
                do_scheduling(i);

            num_searched++;
        }
        for (uint32_t i=0; i<limit; i++) { 
            ooo_model_instr &rob_entry = ROB.entry.at(i);
            if ((rob_entry.fetched != COMPLETED) || (rob_entry.event_cycle > current_core_cycle[cpu]) || (num_searched >= SCHEDULER_SIZE))
                return;

            if (rob_entry.scheduled == 0)
                do_scheduling(i);

            num_searched++;
        }
    }
}

void O3_CPU::do_scheduling(uint32_t rob_index)
{
    ooo_model_instr &rob_entry = ROB.entry.at(rob_index);
    rob_entry.reg_ready = 1; // reg_ready will be reset to 0 if there is RAW dependency 

    reg_dependency(rob_index);
    ROB.next_schedule = (rob_index == (ROB.SIZE - 1)) ? 0 : (rob_index + 1);

    if (rob_entry.is_memory)
        rob_entry.scheduled = INFLIGHT;
    else {
        rob_entry.scheduled = COMPLETED;

        // ADD LATENCY
        if (rob_entry.event_cycle < current_core_cycle[cpu])
            rob_entry.event_cycle = current_core_cycle[cpu] + SCHEDULING_LATENCY;
        else
            rob_entry.event_cycle += SCHEDULING_LATENCY;

        if (rob_entry.reg_ready) {

            // remember this rob_index in the Ready-To-Execute array 1
            while (RTE1.size() >= ROB_SIZE)
                RTE1.pop();
            RTE1.push(rob_index);

            DP (if (warmup_complete[cpu]) {
            cout << "[RTE1] " << __func__ << " instr_id: " << rob_entry.instr_id << " rob_index: " << rob_index << " is added to RTE1";
            });
        }
    }
}

void O3_CPU::reg_dependency(uint32_t rob_index)
{
    ooo_model_instr &rob_entry = ROB.entry.at(rob_index);
    // print out source/destination registers
    DP (if (warmup_complete[cpu]) {
    for (uint32_t i=0; i<NUM_INSTR_SOURCES; i++) {
        if (rob_entry.source_registers[i])
        {
            std::cout << "[ROB] " << __func__ << " instr_id: " << rob_entry.instr_id << " is_memory: " << +rob_entry.is_memory;
            std::cout << " load  reg_index: " << +rob_entry.source_registers[i] << std::endl;
        }
    }
    for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
        if (rob_entry.destination_registers[i])
        {
            std::cout << "[ROB] " << __func__ << " instr_id: " << rob_entry.instr_id << " is_memory: " << +rob_entry.is_memory;
            std::cout << " store reg_index: " << +rob_entry.destination_registers[i] << std::endl;
        }
    } }); 

    // check RAW dependency
    int prior = rob_index - 1;
    if (prior < 0)
        prior = ROB.SIZE - 1;

    if (rob_index != ROB.head)
    {
        if ((int)ROB.head <= prior)
        {
            for (int i=prior; i>=(int)ROB.head; i--)
            {
                if (ROB.entry.at(i).executed != COMPLETED)
                {
                    for (uint32_t j=0; j<NUM_INSTR_SOURCES; j++)
                    {
                        if (rob_entry.source_registers[j] && (rob_entry.reg_RAW_checked[j] == 0))
                            reg_RAW_dependency(i, rob_index, j);
                    }
                }
            }
        }
        else
        {
            for (int i=prior; i>=0; i--)
            {
                if (ROB.entry.at(i).executed != COMPLETED)
                {
                    for (uint32_t j=0; j<NUM_INSTR_SOURCES; j++)
                    {
                        if (rob_entry.source_registers[j] && (rob_entry.reg_RAW_checked[j] == 0))
                            reg_RAW_dependency(i, rob_index, j);
                    }
                }
                for (int i=ROB.SIZE-1; i>=(int)ROB.head; i--)
                {
                    if (ROB.entry.at(i).executed != COMPLETED)
                    {
                        for (uint32_t j=0; j<NUM_INSTR_SOURCES; j++)
                        {
                            if (rob_entry.source_registers[j] && (rob_entry.reg_RAW_checked[j] == 0))
                                reg_RAW_dependency(i, rob_index, j);
                        }
                    }
                }
            }
        }
    }
}

void O3_CPU::reg_RAW_dependency(uint32_t prior, uint32_t current, uint32_t source_index)
{
    ooo_model_instr &prior_entry = ROB.entry.at(prior);
    ooo_model_instr &current_entry = ROB.entry.at(current);
    for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
        if (prior_entry.destination_registers[i] == 0)
            continue;

        if (prior_entry.destination_registers[i] == current_entry.source_registers[source_index]) {

            // we need to mark this dependency in the ROB since the producer might not be added in the store queue yet
            prior_entry.registers_instrs_depend_on_me.insert (current);   // this load cannot be executed until the prior store gets executed
            prior_entry.registers_index_depend_on_me[source_index].insert (current);   // this load cannot be executed until the prior store gets executed
            prior_entry.reg_RAW_producer = 1;

            current_entry.reg_ready = 0;
            current_entry.producer_id = prior_entry.instr_id;
            current_entry.num_reg_dependent++;
            current_entry.reg_RAW_checked[source_index] = 1;

            DP (if(warmup_complete[cpu]) {
                    std::cout << "[ROB] " << __func__ << " instr_id: " << current_entry.instr_id << " is_memory: " << +current_entry.is_memory;
                    std::cout << " RAW reg_index: " << +current_entry.source_registers[source_index];
                    std::cout << " producer_id: " << prior_entry.instr_id << std::endl; });

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
        if (!RTE0.empty()) {
            uint32_t exec_index = RTE0.front();
            if (ROB.entry.at(exec_index).event_cycle <= current_core_cycle[cpu]) {
                do_execution(exec_index);

                RTE0.pop();
                exec_issued++;
            }
        }
        else {
            //DP (if (warmup_complete[cpu]) {
            //cout << "[RTE0] is empty head: " << RTE0_head << " tail: " << RTE0_tail << endl; });
            break;
        }

        num_iteration++;
        if (num_iteration == (ROB_SIZE-1))
            break;
    }

    num_iteration = 0;
    while (exec_issued < EXEC_WIDTH) {
        if (!RTE1.empty()) {
            uint32_t exec_index = RTE1.front();
            if (ROB.entry.at(exec_index).event_cycle <= current_core_cycle[cpu]) {
                do_execution(exec_index);

                RTE1.pop();
                exec_issued++;
            }
        }
        else {
            //DP (if (warmup_complete[cpu]) {
            //cout << "[RTE1] is empty head: " << RTE1_head << " tail: " << RTE1_tail << endl; });
            break;
        }

        num_iteration++;
        if (num_iteration == (ROB_SIZE-1))
            break;
    }
}

void O3_CPU::do_execution(uint32_t rob_index)
{
    ooo_model_instr &rob_entry = ROB.entry.at(rob_index);
    //if (rob_entry.reg_ready && (rob_entry.scheduled == COMPLETED) && (rob_entry.event_cycle <= current_core_cycle[cpu]))
    //{

  //cout << "do_execution() rob_index: " << rob_index << " cycle: " << current_core_cycle[cpu] << endl;
  
        rob_entry.executed = INFLIGHT;

        // ADD LATENCY
        if (rob_entry.event_cycle < current_core_cycle[cpu])
            rob_entry.event_cycle = current_core_cycle[cpu] + EXEC_LATENCY;
        else
            rob_entry.event_cycle += EXEC_LATENCY;

        inflight_reg_executions++;

        DP (if (warmup_complete[cpu]) {
        cout << "[ROB] " << __func__ << " non-memory instr_id: " << rob_entry.instr_id;
        cout << " event_cycle: " << rob_entry.event_cycle << endl;});
    //}
}

void O3_CPU::schedule_memory_instruction()
{
    if ((ROB.head == ROB.tail) && ROB.occupancy == 0)
        return;

    // execution is out-of-order but we have an in-order scheduling algorithm to detect all RAW dependencies
    uint32_t limit = ROB.next_schedule;
    num_searched = 0;
    if (ROB.head < limit) {
        for (uint32_t i=ROB.head; i<limit; i++) {
            ooo_model_instr &rob_entry = ROB.entry.at(i);

            if (rob_entry.is_memory == 0)
                continue;

            if ((rob_entry.fetched != COMPLETED) || (rob_entry.event_cycle > current_core_cycle[cpu]) || (num_searched >= SCHEDULER_SIZE))
                break;

            if (rob_entry.is_memory && rob_entry.reg_ready && (rob_entry.scheduled == INFLIGHT))
                do_memory_scheduling(i);
        }
    }
    else {
        for (uint32_t i=ROB.head; i<ROB.SIZE; i++) {
            ooo_model_instr &rob_entry = ROB.entry.at(i);

            if (rob_entry.is_memory == 0)
                continue;

            if ((rob_entry.fetched != COMPLETED) || (rob_entry.event_cycle > current_core_cycle[cpu]) || (num_searched >= SCHEDULER_SIZE))
                break;

            if (rob_entry.is_memory && rob_entry.reg_ready && (rob_entry.scheduled == INFLIGHT))
                do_memory_scheduling(i);
        }
        for (uint32_t i=0; i<limit; i++) {
            ooo_model_instr &rob_entry = ROB.entry.at(i);

            if (rob_entry.is_memory == 0)
                continue;

            if ((rob_entry.fetched != COMPLETED) || (rob_entry.event_cycle > current_core_cycle[cpu]) || (num_searched >= SCHEDULER_SIZE))
                break;

            if (rob_entry.is_memory && rob_entry.reg_ready && (rob_entry.scheduled == INFLIGHT))
                do_memory_scheduling(i);
        }
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
        ooo_model_instr &rob_entry = ROB.entry.at(rob_index);
        rob_entry.scheduled = COMPLETED;
        if (rob_entry.executed == 0) // it could be already set to COMPLETED due to store-to-load forwarding
            rob_entry.executed  = INFLIGHT;

        DP (if (warmup_complete[cpu]) {
                std::cout << "[ROB] " << __func__ << " instr_id: " << rob_entry.instr_id << " rob_index: " << rob_index;
                std::cout << " scheduled all num_mem_ops: " << rob_entry.num_mem_ops << std::endl; });
    }

    num_searched++;
}

uint32_t O3_CPU::check_and_add_lsq(uint32_t rob_index) 
{
    uint32_t num_mem_ops = 0, num_added = 0;
    ooo_model_instr &rob_entry = ROB.entry.at(rob_index);

    // load
    for (uint32_t i=0; i<NUM_INSTR_SOURCES; i++) {
        if (rob_entry.source_memory[i])
        {
            num_mem_ops++;
            if (rob_entry.source_added[i])
                num_added++;
            else if (LQ.occupancy < LQ_SIZE) {
                add_load_queue(rob_index, i);
                num_added++;
            }
            else {
                DP(if(warmup_complete[cpu]) {
                        std::cout << "[LQ] " << __func__ << " instr_id: " << rob_entry.instr_id;
                        std::cout << " cannot be added in the load queue occupancy: " << LQ.occupancy << " cycle: " << current_core_cycle[cpu] << std::endl; });
            }
        }
    }

    // store
    for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
        if (rob_entry.destination_memory[i])
        {
            num_mem_ops++;
            if (rob_entry.destination_added[i])
                num_added++;
            else if (SQ.occupancy < SQ.SIZE) {
                if (STA.front() == rob_entry.instr_id)
                {
                    add_store_queue(rob_index, i);
                    num_added++;
                }
                //add_store_queue(rob_index, i);
                //num_added++;
            }
            else {
                DP(if(warmup_complete[cpu]) {
                        std::cout << "[SQ] " << __func__ << " instr_id: " << rob_entry.instr_id;
                std::cout << " cannot be added in the store queue occupancy: " << SQ.occupancy << " cycle: " << current_core_cycle[cpu] << std::endl; });
            }
        }
    }

    if (num_added == num_mem_ops)
        return 0;

    uint32_t not_available = num_mem_ops - num_added;
    if (not_available > num_mem_ops) {
        std::cerr << "instr_id: " << rob_entry.instr_id << std::endl;
        assert(0);
    }

    return not_available;
}

void O3_CPU::add_load_queue(uint32_t rob_index, uint32_t data_index)
{
    ooo_model_instr &rob_entry = ROB.entry.at(rob_index);

    // search for an empty slot 
    uint32_t lq_index = LQ_SIZE;
    for (uint32_t i=0; i<LQ_SIZE; i++) {
        if (LQ.entry.at(i).virtual_address == 0) {
            lq_index = i;
            break;
        }
    }

    // sanity check
    if (lq_index == LQ_SIZE) {
        std::cerr << "instr_id: " << rob_entry.instr_id << " no empty slot in the load queue!!!" << std::endl;
        assert(0);
    }

    LSQ_ENTRY &lq_entry = LQ.entry.at(lq_index);

    // add it to the load queue
    rob_entry.lq_index[data_index] = lq_index;
    lq_entry.instr_id = rob_entry.instr_id;
    lq_entry.virtual_address = rob_entry.source_memory[data_index];
    lq_entry.ip = rob_entry.ip;
    lq_entry.data_index = data_index;
    lq_entry.rob_index = rob_index;
    lq_entry.asid[0] = rob_entry.asid[0];
    lq_entry.asid[1] = rob_entry.asid[1];
    lq_entry.event_cycle = current_core_cycle[cpu] + SCHEDULING_LATENCY;
    LQ.occupancy++;

    // check RAW dependency
    int prior = rob_index - 1;
    if (prior < 0)
        prior = ROB.SIZE - 1;

    if (rob_index != ROB.head) {
        if ((int)ROB.head <= prior) {
            for (int i=prior; i>=(int)ROB.head; i--) {
                if (lq_entry.producer_id != UINT64_MAX)
                    break;

                    mem_RAW_dependency(i, rob_index, data_index, lq_index);
            }
        }
        else {
            for (int i=prior; i>=0; i--) {
                if (lq_entry.producer_id != UINT64_MAX)
                    break;

                    mem_RAW_dependency(i, rob_index, data_index, lq_index);
            }
            for (int i=ROB.SIZE-1; i>=(int)ROB.head; i--) { 
                if (lq_entry.producer_id != UINT64_MAX)
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
        LSQ_ENTRY &sq_check_entry = SQ.entry.at(i);

        // skip empty slot
        if (sq_check_entry.virtual_address == 0)
            continue;

        // forwarding should be done by the SQ entry that holds the same producer_id from RAW dependency check
        if (sq_check_entry.virtual_address == lq_entry.virtual_address) { // store-to-load forwarding check

            // forwarding store is in the SQ
            if ((rob_index != ROB.head) && (lq_entry.producer_id == sq_check_entry.instr_id)) { // RAW
                forwarding_index = i;
                break; // should be break
            }

            if ((lq_entry.producer_id == UINT64_MAX) && (lq_entry.instr_id <= sq_check_entry.instr_id)) { // WAR 
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

                lq_entry.physical_address = 0;
                lq_entry.translated = 0;
                lq_entry.fetched = 0;
                
                DP(if(warmup_complete[cpu]) {
                        std::cout << "[LQ] " << __func__ << " instr_id: " << lq_entry.instr_id << " reset fetched: " << +lq_entry.fetched;
                        std::cout << " to obey WAR store instr_id: " << sq_check_entry.instr_id << " cycle: " << current_core_cycle[cpu] << std::endl; });
            }
        }
    }

    if (forwarding_index != SQ.SIZE) { // we have a store-to-load forwarding
        LSQ_ENTRY &sq_entry = SQ.entry.at(forwarding_index);

        if ((sq_entry.fetched == COMPLETED) && (sq_entry.event_cycle <= current_core_cycle[cpu])) {
            lq_entry.physical_address = (sq_entry.physical_address & ~(uint64_t) ((1 << LOG2_BLOCK_SIZE) - 1)) | (lq_entry.virtual_address & ((1 << LOG2_BLOCK_SIZE) - 1));
            lq_entry.translated = COMPLETED;
            lq_entry.fetched = COMPLETED;

            ooo_model_instr &fwr_rob_entry = ROB.entry.at(lq_entry.rob_index);
            fwr_rob_entry.num_mem_ops--;
            fwr_rob_entry.event_cycle = current_core_cycle[cpu];
            if (fwr_rob_entry.num_mem_ops < 0) {
                std::cerr << "instr_id: " << fwr_rob_entry.instr_id << std::endl;
                assert(0);
            }
            if (fwr_rob_entry.num_mem_ops == 0)
                inflight_mem_executions++;

            DP(if(warmup_complete[cpu]) {
                    std::cout << "[LQ] " << __func__ << " instr_id: " << lq_entry.instr_id << std::hex;
                    std::cout << " full_addr: " << lq_entry.physical_address << std::dec << " is forwarded by store instr_id: ";
                    std::cout << sq_entry.instr_id << " remain_num_ops: " << fwr_rob_entry.num_mem_ops << " cycle: " << current_core_cycle[cpu] << std::endl; });

            release_load_queue(lq_index);
        }
        else
            ; // store is not executed yet, forwarding will be handled by execute_store()
    }

    // succesfully added to the load queue
    rob_entry.source_added[data_index] = 1;

    if (lq_entry.virtual_address && (lq_entry.producer_id == UINT64_MAX)) { // not released and no forwarding
        while (RTL0.size() >= LQ_SIZE)
            RTL0.pop();
        RTL0.push(lq_index);

        DP (if (warmup_complete[cpu]) {
                std::cout << "[RTL0] " << __func__ << " instr_id: " << lq_entry.instr_id << " rob_index: " << lq_entry.rob_index << " is added to RTL0";
                 });
    }

    DP(if(warmup_complete[cpu]) {
            std::cout << "[LQ] " << __func__ << " instr_id: " << lq_entry.instr_id;
            std::cout << " is added in the LQ address: " << std::hex << lq_entry.virtual_address << std::dec << " translated: " << +lq_entry.translated;
            std::cout << " fetched: " << +lq_entry.fetched << " index: " << lq_index << " occupancy: " << LQ.occupancy << " cycle: " << current_core_cycle[cpu] << std::endl; });
}

void O3_CPU::mem_RAW_dependency(uint32_t prior, uint32_t current, uint32_t data_index, uint32_t lq_index)
{
    ooo_model_instr &prior_entry = ROB.entry.at(prior);
    ooo_model_instr &current_entry = ROB.entry.at(current);
    LSQ_ENTRY &lq_entry = LQ.entry.at(lq_index);

    for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
        if (prior_entry.destination_memory[i] == 0)
            continue;

        if (prior_entry.destination_memory[i] == current_entry.source_memory[data_index]) { //  store-to-load forwarding check

            // we need to mark this dependency in the ROB since the producer might not be added in the store queue yet
            prior_entry.memory_instrs_depend_on_me.insert (current);   // this load cannot be executed until the prior store gets executed
            prior_entry.is_producer = 1;
            lq_entry.producer_id = prior_entry.instr_id;
            lq_entry.translated = INFLIGHT;

            DP (if(warmup_complete[cpu]) {
                    std::cout << "[LQ] " << __func__ << " RAW producer instr_id: " << prior_entry.instr_id << " consumer_id: " << current_entry.instr_id << " lq_index: " << lq_index;
                    std::cout << std::hex << " address: " << prior_entry.destination_memory[i] << std::dec << std::endl; });

            return;
        }
    }
}

void O3_CPU::add_store_queue(uint32_t rob_index, uint32_t data_index)
{
    ooo_model_instr &rob_entry = ROB.entry.at(rob_index);
    uint32_t sq_index = SQ.tail;

    /*
    // search for an empty slot 
    for (uint32_t i=0; i<SQ.SIZE; i++) {
        if (SQ.entry.at(i).virtual_address == 0) {
            sq_index = i;
            break;
        }
    }

    // sanity check
    if (sq_index == SQ.SIZE) {
        cerr << "instr_id: " << rob_entry.instr_id << " no empty slot in the store queue!!!" << endl;
        assert(0);
    }
    */

    LSQ_ENTRY &sq_entry = SQ.entry.at(sq_index);
    assert(sq_entry.virtual_address == 0);

    // add it to the store queue
    rob_entry.sq_index[data_index] = sq_index;
    sq_entry.instr_id = rob_entry.instr_id;
    sq_entry.virtual_address = rob_entry.destination_memory[data_index];
    sq_entry.ip = rob_entry.ip;
    sq_entry.data_index = data_index;
    sq_entry.rob_index = rob_index;
    sq_entry.asid[0] = rob_entry.asid[0];
    sq_entry.asid[1] = rob_entry.asid[1];
    sq_entry.event_cycle = current_core_cycle[cpu] + SCHEDULING_LATENCY;

    SQ.occupancy++;
    SQ.tail++;
    if (SQ.tail == SQ.SIZE)
        SQ.tail = 0;

    // succesfully added to the store queue
    rob_entry.destination_added[data_index] = 1;

    STA.pop();

    while (RTS0.size() >= SQ_SIZE)
        RTS0.pop();
    RTS0.push(sq_index);

    DP(if(warmup_complete[cpu]) {
            std::cout << "[SQ] " << __func__ << " instr_id: " << sq_entry.instr_id;
            std::cout << " is added in the SQ translated: " << +sq_entry.translated << " fetched: " << +sq_entry.fetched << " is_producer: " << +rob_entry.is_producer;
            std::cout << " cycle: " << current_core_cycle[cpu] << std::endl; });
}

void O3_CPU::operate_lsq()
{
    // handle store
    uint32_t store_issued = 0, num_iteration = 0;

    while (store_issued < SQ_WIDTH) {
        if (!RTS0.empty()) {
            LSQ_ENTRY &sq_entry = SQ.entry.at(RTS0.front());
            if (sq_entry.event_cycle <= current_core_cycle[cpu]) {

                // add it to DTLB
                PACKET data_packet;

                data_packet.tlb_access = 1;
                data_packet.fill_level = FILL_L1;
                data_packet.fill_l1d = 1;
                data_packet.cpu = cpu;
                data_packet.data_index = sq_entry.data_index;
                data_packet.sq_index = RTS0.front();
                if (knob_cloudsuite)
                    data_packet.address = ((sq_entry.virtual_address >> LOG2_PAGE_SIZE) << 9) | sq_entry.asid[1];
                else
                    data_packet.address = sq_entry.virtual_address >> LOG2_PAGE_SIZE;
                data_packet.full_addr = sq_entry.virtual_address;
                data_packet.instr_id = sq_entry.instr_id;
                data_packet.rob_index = sq_entry.rob_index;
                data_packet.ip = sq_entry.ip;
                data_packet.type = RFO;
                data_packet.asid[0] = sq_entry.asid[0];
                data_packet.asid[1] = sq_entry.asid[1];
                data_packet.event_cycle = sq_entry.event_cycle;

                DP (if (warmup_complete[cpu]) {
                        std::cout << "[RTS0] " << __func__ << " instr_id: " << sq_entry.instr_id << " rob_index: " << sq_entry.rob_index << " is popped from to RTS0";
                         });

                int rq_index = DTLB.add_rq(&data_packet);

                if (rq_index == -2)
                    break; 
                else 
                    sq_entry.translated = INFLIGHT;

                RTS0.pop();

                store_issued++;
            }
        }
        else {
            break;
        }

        num_iteration++;
        if (num_iteration == (SQ_SIZE-1))
            break;
    }

    num_iteration = 0;
    while (store_issued < SQ_WIDTH) {
        if (!RTS1.empty()) {
            LSQ_ENTRY &sq_entry = SQ.entry.at(RTS1.front());
            if (sq_entry.event_cycle <= current_core_cycle[cpu]) {
                execute_store(sq_entry.rob_index, RTS1.front(), sq_entry.data_index);

                RTS1.pop();

                store_issued++;
            }
        }
        else {
            break;
        }

        num_iteration++;
        if (num_iteration == (SQ_SIZE-1))
            break;
    }

    unsigned load_issued = 0;
    num_iteration = 0;
    while (load_issued < LQ_WIDTH) {
        if (!RTL0.empty()) {
            LSQ_ENTRY &lq_entry = LQ.entry.at(RTL0.front());
            if (lq_entry.event_cycle <= current_core_cycle[cpu]) {

                // add it to DTLB
                PACKET data_packet;
                data_packet.fill_level = FILL_L1;
                data_packet.fill_l1d = 1;
                data_packet.cpu = cpu;
                data_packet.data_index = lq_entry.data_index;
                data_packet.lq_index = RTL0.front();
                if (knob_cloudsuite)
                    data_packet.address = ((lq_entry.virtual_address >> LOG2_PAGE_SIZE) << 9) | lq_entry.asid[1];
                else
                    data_packet.address = lq_entry.virtual_address >> LOG2_PAGE_SIZE;
                data_packet.full_addr = lq_entry.virtual_address;
                data_packet.instr_id = lq_entry.instr_id;
                data_packet.rob_index = lq_entry.rob_index;
                data_packet.ip = lq_entry.ip;
                data_packet.type = LOAD;
                data_packet.asid[0] = lq_entry.asid[0];
                data_packet.asid[1] = lq_entry.asid[1];
                data_packet.event_cycle = lq_entry.event_cycle;

                DP (if (warmup_complete[cpu]) {
                        std::cout << "[RTL0] " << __func__ << " instr_id: " << lq_entry.instr_id << " rob_index: " << lq_entry.rob_index << " is popped to RTL0";
                         });

                int rq_index = DTLB.add_rq(&data_packet);

                if (rq_index == -2)
                    break; // break here
                else  
                    lq_entry.translated = INFLIGHT;

                RTL0.pop();

                load_issued++;
            }
        }
        else {
            break;
        }

        num_iteration++;
        if (num_iteration == (LQ_SIZE-1))
            break;
    }

    num_iteration = 0;
    while (load_issued < LQ_WIDTH) {
        if (!RTL1.empty()) {
            LSQ_ENTRY &lq_entry = LQ.entry.at(RTL1.front());
            if (lq_entry.event_cycle <= current_core_cycle[cpu]) {
                int rq_index = execute_load(lq_entry.rob_index, RTL1.front(), lq_entry.data_index);

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
        if (num_iteration == (LQ_SIZE-1))
            break;
    }
}

void O3_CPU::execute_store(uint32_t rob_index, uint32_t sq_index, uint32_t data_index)
{
    ooo_model_instr &rob_entry = ROB.entry.at(rob_index);
    LSQ_ENTRY &sq_entry = SQ.entry.at(sq_index);

    sq_entry.fetched = COMPLETED;
    sq_entry.event_cycle = current_core_cycle[cpu];

    rob_entry.num_mem_ops--;
    rob_entry.event_cycle = current_core_cycle[cpu];
    if (rob_entry.num_mem_ops < 0) {
        std::cerr << "instr_id: " << rob_entry.instr_id << std::endl;
        assert(0);
    }
    if (rob_entry.num_mem_ops == 0)
        inflight_mem_executions++;

    DP (if (warmup_complete[cpu]) {
            std::cout << "[SQ1] " << __func__ << " instr_id: " << sq_entry.instr_id << std::hex;
            std::cout << " full_address: " << sq_entry.physical_address << std::dec << " remain_mem_ops: " << rob_entry.num_mem_ops;
            std::cout << " event_cycle: " << sq_entry.event_cycle << std::endl; });

    // resolve RAW dependency after DTLB access
    // check if this store has dependent loads
    if (rob_entry.is_producer)
    {
        ITERATE_SET(dependent,rob_entry.memory_instrs_depend_on_me, ROB_SIZE)
        {
            ooo_model_instr &rob_dependent = ROB.entry.at(dependent);
            // check if dependent loads are already added in the load queue
            for (uint32_t j=0; j<NUM_INSTR_SOURCES; j++) { // which one is dependent?
                if (rob_dependent.source_memory[j] && rob_dependent.source_added[j]) {
                    if (rob_dependent.source_memory[j] == sq_entry.virtual_address) { // this is required since a single instruction can issue multiple loads

                        // now we can resolve RAW dependency
                        LSQ_ENTRY &lq_entry = LQ.entry.at(rob_dependent.lq_index[j]);
#ifdef SANITY_CHECK
                        if (lq_entry.producer_id != sq_entry.instr_id) {
                            std::cerr << "[SQ2] " << __func__ << " lq_index: " << rob_dependent.lq_index[j] << " producer_id: " << lq_entry.producer_id;
                            std::cerr << " does not match to the store instr_id: " << sq_entry.instr_id << std::endl;
                            assert(0);
                        }
#endif
                        // update correspodning LQ entry
                        lq_entry.physical_address = (sq_entry.physical_address & ~(uint64_t) ((1 << LOG2_BLOCK_SIZE) - 1)) | (lq_entry.virtual_address & ((1 << LOG2_BLOCK_SIZE) - 1));
                        lq_entry.translated = COMPLETED;
                        lq_entry.fetched = COMPLETED;
                        lq_entry.event_cycle = current_core_cycle[cpu];

                        ooo_model_instr &fwr_rob_entry = ROB.entry.at(lq_entry.rob_index);
                        fwr_rob_entry.num_mem_ops--;
                        fwr_rob_entry.event_cycle = current_core_cycle[cpu];
#ifdef SANITY_CHECK
                        if (fwr_rob_entry.num_mem_ops < 0) {
                            cerr << "instr_id: " << fwr_rob_entry.instr_id << endl;
                            assert(0);
                        }
#endif
                        if (fwr_rob_entry.num_mem_ops == 0)
                            inflight_mem_executions++;

                        DP(if(warmup_complete[cpu]) {
                                std::cout << "[LQ3] " << __func__ << " instr_id: " << lq_entry.instr_id << std::hex;
                                std::cout << " full_addr: " << lq_entry.physical_address << std::dec << " is forwarded by store instr_id: ";
                                std::cout << sq_entry.instr_id << " remain_num_ops: " << fwr_rob_entry.num_mem_ops << " cycle: " << current_core_cycle[cpu] << std::endl; });

                        release_load_queue(rob_dependent.lq_index[j]);

                        // clear dependency bit
                        if (j == (NUM_INSTR_SOURCES-1))
                            rob_entry.memory_instrs_depend_on_me.insert (dependent);
                    }
                }
            }
        }
    }
}

int O3_CPU::execute_load(uint32_t rob_index, uint32_t lq_index, uint32_t data_index)
{
    LSQ_ENTRY &lq_entry = LQ.entry.at(lq_index);

    // add it to L1D
    PACKET data_packet;
    data_packet.fill_level = FILL_L1;
    data_packet.fill_l1d = 1;
    data_packet.cpu = cpu;
    data_packet.data_index = lq_entry.data_index;
    data_packet.lq_index = lq_index;
    data_packet.address = lq_entry.physical_address >> LOG2_BLOCK_SIZE;
    data_packet.full_addr = lq_entry.physical_address;
    data_packet.instr_id = lq_entry.instr_id;
    data_packet.rob_index = lq_entry.rob_index;
    data_packet.ip = lq_entry.ip;
    data_packet.type = LOAD;
    data_packet.asid[0] = lq_entry.asid[0];
    data_packet.asid[1] = lq_entry.asid[1];
    data_packet.event_cycle = lq_entry.event_cycle;

    int rq_index = L1D.add_rq(&data_packet);

    if (rq_index == -2)
        return rq_index;
    else 
        lq_entry.fetched = INFLIGHT;

    return rq_index;
}

void O3_CPU::complete_execution(uint32_t rob_index)
{
    ooo_model_instr &rob_entry = ROB.entry.at(rob_index);

    if (rob_entry.is_memory == 0) {
        if ((rob_entry.executed == INFLIGHT) && (rob_entry.event_cycle <= current_core_cycle[cpu])) {

            rob_entry.executed = COMPLETED;
            inflight_reg_executions--;
            completed_executions++;

            if (rob_entry.reg_RAW_producer)
                reg_RAW_release(rob_index);

            if (rob_entry.branch_mispredicted)
	      {
		fetch_resume_cycle = current_core_cycle[cpu] + BRANCH_MISPREDICT_PENALTY;
	      }

            DP(if(warmup_complete[cpu]) {
                    std::cout << "[ROB] " << __func__ << " instr_id: " << rob_entry.instr_id;
                    std::cout << " branch_mispredicted: " << +rob_entry.branch_mispredicted << " fetch_stall: " << +fetch_stall;
                    std::cout << " event: " << rob_entry.event_cycle << std::endl; });
        }
    }
    else {
        if (rob_entry.num_mem_ops == 0) {
            if ((rob_entry.executed == INFLIGHT) && (rob_entry.event_cycle <= current_core_cycle[cpu])) {

                rob_entry.executed = COMPLETED;
                inflight_mem_executions--;
                completed_executions++;

                if (rob_entry.reg_RAW_producer)
                    reg_RAW_release(rob_index);

                if (rob_entry.branch_mispredicted)
		  {
		    fetch_resume_cycle = current_core_cycle[cpu] + BRANCH_MISPREDICT_PENALTY;
		  }

                DP(if(warmup_complete[cpu]) {
                        std::cout << "[ROB] " << __func__ << " instr_id: " << rob_entry.instr_id;
                        std::cout << " is_memory: " << +rob_entry.is_memory << " branch_mispredicted: " << +rob_entry.branch_mispredicted;
                        std::cout << " fetch_stall: " << +fetch_stall << " event: " << rob_entry.event_cycle << " current: " << current_core_cycle[cpu] << std::endl; });
            }
        }
    }
}

void O3_CPU::reg_RAW_release(uint32_t rob_index)
{
    // if (!ROB.entry.at(rob_index).registers_instrs_depend_on_me.empty()) 

    ITERATE_SET(i,ROB.entry.at(rob_index).registers_instrs_depend_on_me, ROB_SIZE) {
        for (uint32_t j=0; j<NUM_INSTR_SOURCES; j++) {
            if (ROB.entry.at(rob_index).registers_index_depend_on_me[j].search (i)) {
                ooo_model_instr &rob_entry = ROB.entry.at(i);
                rob_entry.num_reg_dependent--;

                if (rob_entry.num_reg_dependent == 0) {
                    rob_entry.reg_ready = 1;
                    if (rob_entry.is_memory)
                        rob_entry.scheduled = INFLIGHT;
                    else {
                        rob_entry.scheduled = COMPLETED;

                        while (RTE0.size() >= ROB_SIZE)
                            RTE0.pop();
                        RTE0.push(i);

                        DP (if (warmup_complete[cpu]) {
                                std::cout << "[RTE0] " << __func__ << " instr_id: " << rob_entry.instr_id << " rob_index: " << i << " is added to RTE0";
                                 });
                    }
                }

                DP (if (warmup_complete[cpu]) {
                        std::cout << "[ROB] " << __func__ << " instr_id: " << ROB.entry.at(rob_index).instr_id << " releases instr_id: ";
                        std::cout << rob_entry.instr_id << " reg_index: " << +rob_entry.source_registers[j] << " num_reg_dependent: " << rob_entry.num_reg_dependent << " cycle: " << current_core_cycle[cpu] << std::endl; });
            }
        }
    }
}

void O3_CPU::operate_cache()
{
    ITLB.operate();
    DTLB.operate();
    STLB.operate();
    L1I.operate();
    L1D.operate();
    L2C.operate();

    // also handle per-cycle prefetcher operation
    l1i_prefetcher_cycle_operate();
}

void O3_CPU::update_rob()
{
    if (ITLB.PROCESSED.occupancy && (ITLB.PROCESSED.entry[ITLB.PROCESSED.head].event_cycle <= current_core_cycle[cpu]))
        complete_instr_fetch(&ITLB.PROCESSED, 1);

    if (L1I.PROCESSED.occupancy && (L1I.PROCESSED.entry[L1I.PROCESSED.head].event_cycle <= current_core_cycle[cpu]))
        complete_instr_fetch(&L1I.PROCESSED, 0);

    if (DTLB.PROCESSED.occupancy && (DTLB.PROCESSED.entry[DTLB.PROCESSED.head].event_cycle <= current_core_cycle[cpu]))
        complete_data_fetch(&DTLB.PROCESSED, 1);

    if (L1D.PROCESSED.occupancy && (L1D.PROCESSED.entry[L1D.PROCESSED.head].event_cycle <= current_core_cycle[cpu]))
        complete_data_fetch(&L1D.PROCESSED, 0);

    // update ROB entries with completed executions
    if ((inflight_reg_executions > 0) || (inflight_mem_executions > 0)) {
        if (ROB.head < ROB.tail) {
            for (uint32_t i=ROB.head; i<ROB.tail; i++) 
                complete_execution(i);
        }
        else {
            for (uint32_t i=ROB.head; i<ROB.SIZE; i++)
                complete_execution(i);
            for (uint32_t i=0; i<ROB.tail; i++)
                complete_execution(i);
        }
    }
}

void O3_CPU::complete_instr_fetch(PACKET_QUEUE *queue, uint8_t is_it_tlb)
{
    PACKET &head_packet = queue->entry.at(queue->head);

    uint64_t complete_ip = head_packet.ip;

    if(is_it_tlb)
    {
        //uint64_t instruction_physical_address = (head_packet.instruction_pa << LOG2_PAGE_SIZE) | (complete_ip & ((1 << LOG2_PAGE_SIZE) - 1));

        // mark the appropriate instructions in the IFETCH_BUFFER as translated and ready to fetch
        for(uint32_t j=0; j<IFETCH_BUFFER.SIZE; j++) //FIXME this may scan invalid entries
        {
            ooo_model_instr &ifb_entry = IFETCH_BUFFER.entry.at(j);
            if(ifb_entry.ip>>LOG2_PAGE_SIZE == complete_ip>>LOG2_PAGE_SIZE)
            {
                ifb_entry.translated = COMPLETED;
                // we did not fetch this instruction's cache line, but we did translate it
                ifb_entry.fetched = 0;
                // recalculate a physical address for this cache line based on the translated physical page address
                uint64_t instr_pa = (head_packet.instruction_pa << LOG2_PAGE_SIZE) | (ifb_entry.ip & ((1 << LOG2_PAGE_SIZE) - 1));
                ifb_entry.instruction_pa = instr_pa;
            }
        }

        // remove this entry
        queue->remove_queue(&queue->entry[queue->head]);
      }
    else
      {
          // this is the L1I cache, so instructions are now fully fetched, so mark them as such
          for(uint32_t j=0; j<IFETCH_BUFFER.SIZE; j++)
          {
              ooo_model_instr &ifb_entry = IFETCH_BUFFER.entry.at(j);
              if(ifb_entry.ip>>6 == complete_ip>>6)
              {
                  ifb_entry.translated = COMPLETED;
                  ifb_entry.fetched = COMPLETED;
              }
          }

          // remove this entry
          queue->remove_queue(&queue->entry[queue->head]);
      }

    return;

    // old function below

    ooo_model_instr &rob_entry = ROB.entry.at(head_packet.rob_index);
    unsigned int num_fetched = 0;
#ifdef SANITY_CHECK
    assert(head_packet.rob_index == check_rob(head_packet.instr_id));
#endif

    // update ROB entry
    if (is_it_tlb) {
        rob_entry.translated = COMPLETED;
        rob_entry.instruction_pa = (head_packet.instruction_pa << LOG2_PAGE_SIZE) | (rob_entry.ip & ((1 << LOG2_PAGE_SIZE) - 1)); // translated address
    }
    else
        rob_entry.fetched = COMPLETED;
    rob_entry.event_cycle = current_core_cycle[cpu];
    num_fetched++;

    DP ( if (warmup_complete[cpu]) {
            std::cout << "[" << queue->NAME << "] " << __func__ << " cpu: " << cpu <<  " instr_id: " << rob_entry.instr_id;
            std::cout << " ip: " << std::hex << rob_entry.ip << " address: " << rob_entry.instruction_pa << std::dec;
            std::cout << " translated: " << +rob_entry.translated << " fetched: " << +rob_entry.fetched;
            std::cout << " event_cycle: " << rob_entry.event_cycle << std::endl; });

    // check if other instructions were merged
    if (head_packet.instr_merged) {
	ITERATE_SET(i,head_packet.rob_index_depend_on_me, ROB_SIZE) {
        ooo_model_instr &rob_dependent = ROB.entry.at(i);
            // update ROB entry
            if (is_it_tlb) {
                rob_dependent.translated = COMPLETED;
                rob_dependent.instruction_pa = (head_packet.instruction_pa << LOG2_PAGE_SIZE) | (rob_dependent.ip & ((1 << LOG2_PAGE_SIZE) - 1)); // translated address
            }
            else
                rob_dependent.fetched = COMPLETED;
            rob_dependent.event_cycle = current_core_cycle[cpu] + (num_fetched / FETCH_WIDTH);
            num_fetched++;

            DP ( if (warmup_complete[cpu]) {
                    std::cout << "[" << queue->NAME << "] " << __func__ << " cpu: " << cpu <<  " instr_id: " << rob_dependent.instr_id;
                    std::cout << " ip: " << std::hex << rob_dependent.ip << " address: " << rob_dependent.instruction_pa << std::dec;
                    std::cout << " translated: " << +rob_dependent.translated << " fetched: " << +rob_dependent.fetched << " provider: " << rob_entry.instr_id;
                    std::cout << " event_cycle: " << rob_dependent.event_cycle << std::endl; });
        }
    }

    // remove this entry
    queue->remove_queue(&queue->entry[queue->head]);
}

void O3_CPU::complete_data_fetch(PACKET_QUEUE *queue, uint8_t is_it_tlb)
{
    PACKET &head_packet = queue->entry[queue->head];
    ooo_model_instr &rob_entry = ROB.entry.at(head_packet.rob_index);
    LSQ_ENTRY &sq_entry  = SQ.entry.at(head_packet.sq_index);
    LSQ_ENTRY &lq_entry  = LQ.entry.at(head_packet.lq_index);

    assert(head_packet.type == RFO || head_packet.rob_index == check_rob(head_packet.instr_id));

    // update ROB entry
    if (is_it_tlb) { // DTLB

        if (head_packet.type == RFO)
        {
            sq_entry.physical_address = (head_packet.data_pa << LOG2_PAGE_SIZE) | (sq_entry.virtual_address & ((1 << LOG2_PAGE_SIZE) - 1)); // translated address
            sq_entry.translated = COMPLETED;
            sq_entry.event_cycle = current_core_cycle[cpu];

            while (RTS1.size() >= SQ_SIZE)
                RTS1.pop();
            RTS1.push(head_packet.sq_index);

            DP (if (warmup_complete[cpu]) {
                    std::cout << "[ROB] " << __func__ << " RFO instr_id: " << sq_entry.instr_id;
                    std::cout << " DTLB_FETCH_DONE translation: " << +sq_entry.translated << std::hex << " page: " << (sq_entry.physical_address>>LOG2_PAGE_SIZE);
                    std::cout << " full_addr: " << sq_entry.physical_address << std::dec << " store_merged: " << +head_packet.store_merged;
                    std::cout << " load_merged: " << +head_packet.load_merged << std::endl; });

            handle_merged_translation(&queue->entry[queue->head]);
        }
        else { 
            lq_entry.physical_address = (head_packet.data_pa << LOG2_PAGE_SIZE) | (lq_entry.virtual_address & ((1 << LOG2_PAGE_SIZE) - 1)); // translated address
            lq_entry.translated = COMPLETED;
            lq_entry.event_cycle = current_core_cycle[cpu];

            while (RTL1.size() >= LQ_SIZE)
                RTL1.pop();
            RTL1.push(head_packet.lq_index);

            DP (if (warmup_complete[cpu]) {
                    std::cout << "[RTL1] " << __func__ << " instr_id: " << lq_entry.instr_id << " rob_index: " << lq_entry.rob_index << " is added to RTL1";
                     });

            DP (if (warmup_complete[cpu]) {
                    std::cout << "[ROB] " << __func__ << " load instr_id: " << lq_entry.instr_id;
                    std::cout << " DTLB_FETCH_DONE translation: " << +lq_entry.translated << std::hex << " page: " << (lq_entry.physical_address>>LOG2_PAGE_SIZE);
                    std::cout << " full_addr: " << lq_entry.physical_address << std::dec << " store_merged: " << +head_packet.store_merged;
                    std::cout << " load_merged: " << +head_packet.load_merged << std::endl; });

            handle_merged_translation(&queue->entry[queue->head]);
        }

        rob_entry.event_cycle = head_packet.event_cycle;
    }
    else { // L1D

        if (head_packet.type == RFO)
            handle_merged_load(&queue->entry[queue->head]);
        else { 
#ifdef SANITY_CHECK
            if (head_packet.store_merged)
                assert(0);
#endif
            lq_entry.fetched = COMPLETED;
            lq_entry.event_cycle = current_core_cycle[cpu];
            rob_entry.num_mem_ops--;
            rob_entry.event_cycle = head_packet.event_cycle;

#ifdef SANITY_CHECK
            if (rob_entry.num_mem_ops < 0) {
                std::cerr << "instr_id: " << rob_entry.instr_id << std::endl;
                assert(0);
            }
#endif
            if (rob_entry.num_mem_ops == 0)
                inflight_mem_executions++;

            DP (if (warmup_complete[cpu]) {
                    std::cout << "[ROB] " << __func__ << " load instr_id: " << lq_entry.instr_id;
                    std::cout << " L1D_FETCH_DONE fetched: " << +lq_entry.fetched << std::hex << " address: " << lq_entry.physical_address>>LOG2_BLOCK_SIZE;
                    std::cout << " full_addr: " << lq_entry.physical_address << std::dec << " remain_mem_ops: " << rob_entry.num_mem_ops;
                    std::cout << " load_merged: " << +head_packet.load_merged << " inflight_mem: " << inflight_mem_executions << std::endl; });

            release_load_queue(head_packet.lq_index);
            handle_merged_load(&queue->entry[queue->head]);
        }
    }

    // remove this entry
    queue->remove_queue(&queue->entry[queue->head]);
}

//FIXME unused?
void O3_CPU::handle_o3_fetch(PACKET *current_packet, uint32_t cache_type)
{
    ooo_model_instr &rob_entry = ROB.entry.at(current_packet->rob_index);
    LSQ_ENTRY &sq_entry  = SQ.entry.at(current_packet->sq_index);
    LSQ_ENTRY &lq_entry  = LQ.entry.at(current_packet->lq_index);

    // update ROB entry
    if (cache_type == 0) { // DTLB

        assert(current_packet->rob_index == check_rob(current_packet->instr_id));
        if (current_packet->type == RFO) {
            sq_entry.physical_address = (current_packet->data_pa << LOG2_PAGE_SIZE) | (sq_entry.virtual_address & ((1 << LOG2_PAGE_SIZE) - 1)); // translated address
            sq_entry.translated = COMPLETED;

            while (RTS1.size() >= SQ_SIZE)
                RTS1.pop();
            RTS1.push(current_packet->sq_index);

            DP (if (warmup_complete[cpu]) {
                    std::cout << "[ROB] " << __func__ << " RFO instr_id: " << sq_entry.instr_id;
                    std::cout << " DTLB_FETCH_DONE translation: " << +sq_entry.translated << std::hex << " page: " << sq_entry.physical_address>>LOG2_PAGE_SIZE;
                    std::cout << " full_addr: " << sq_entry.physical_address << std::dec << " store_merged: " << +current_packet->store_merged;
                    std::cout << " load_merged: " << +current_packet->load_merged << std::endl; });

            handle_merged_translation(current_packet);
        }
        else { 
            lq_entry.physical_address = (current_packet->data_pa << LOG2_PAGE_SIZE) | (lq_entry.virtual_address & ((1 << LOG2_PAGE_SIZE) - 1)); // translated address
            lq_entry.translated = COMPLETED;

            while (RTL1.size() >= LQ_SIZE)
                RTL1.pop();
            RTL1.push(current_packet->lq_index);

            DP (if (warmup_complete[cpu]) {
                    std::cout << "[RTL1] " << __func__ << " instr_id: " << lq_entry.instr_id << " rob_index: " << lq_entry.rob_index << " is added to RTL1";
                     });

            DP (if (warmup_complete[cpu]) {
                    std::cout << "[ROB] " << __func__ << " load instr_id: " << lq_entry.instr_id;
                    std::cout << " DTLB_FETCH_DONE translation: " << +lq_entry.translated << std::hex << " page: " << (lq_entry.physical_address>>LOG2_PAGE_SIZE);
                    std::cout << " full_addr: " << lq_entry.physical_address << std::dec << " store_merged: " << +current_packet->store_merged;
                    std::cout << " load_merged: " << +current_packet->load_merged << std::endl; });

            handle_merged_translation(current_packet);
        }

        rob_entry.event_cycle = current_packet->event_cycle;
    }
    else { // L1D

        if (current_packet->type == RFO)
            handle_merged_load(current_packet);
        else { // do traditional things
#ifdef SANITY_CHECK
            if (current_packet->rob_index != check_rob(current_packet->instr_id))
                assert(0);

            if (current_packet->store_merged)
                assert(0);
#endif
            lq_entry.fetched = COMPLETED;
            rob_entry.num_mem_ops--;

#ifdef SANITY_CHECK
            if (rob_entry.num_mem_ops < 0) {
                cerr << "instr_id: " << rob_entry.instr_id << endl;
                assert(0);
            }
#endif
            if (rob_entry.num_mem_ops == 0)
                inflight_mem_executions++;

            DP (if (warmup_complete[cpu]) {
                    std::cout << "[ROB] " << __func__ << " load instr_id: " << lq_entry.instr_id;
                    std::cout << " L1D_FETCH_DONE fetched: " << +lq_entry.fetched << std::hex << " address: " << (lq_entry.physical_address>>LOG2_BLOCK_SIZE);
                    std::cout << " full_addr: " << lq_entry.physical_address << std::dec << " remain_mem_ops: " << rob_entry.num_mem_ops;
                    std::cout << " load_merged: " << +current_packet->load_merged << " inflight_mem: " << inflight_mem_executions << std::endl; });

            release_load_queue(current_packet->lq_index);

            handle_merged_load(current_packet);

            rob_entry.event_cycle = current_packet->event_cycle;
        }
    }
}

void O3_CPU::handle_merged_translation(PACKET *provider)
{
    if (provider->store_merged) {
	ITERATE_SET(merged, provider->sq_index_depend_on_me, SQ.SIZE) {
        LSQ_ENTRY &sq_entry = SQ.entry.at(merged);
            sq_entry.translated = COMPLETED;
            sq_entry.physical_address = (provider->data_pa << LOG2_PAGE_SIZE) | (sq_entry.virtual_address & ((1 << LOG2_PAGE_SIZE) - 1)); // translated address
            sq_entry.event_cycle = current_core_cycle[cpu];

            while (RTS1.size() >= SQ_SIZE)
                RTS1.pop();
            RTS1.push(merged);

            DP (if (warmup_complete[cpu]) {
                    std::cout << "[ROB] " << __func__ << " store instr_id: " << sq_entry.instr_id;
                    std::cout << " DTLB_FETCH_DONE translation: " << +sq_entry.translated << std::hex << " page: " << (sq_entry.physical_address>>LOG2_PAGE_SIZE);
                    std::cout << " full_addr: " << sq_entry.physical_address << std::dec << " by instr_id: " << +provider->instr_id << std::endl; });
        }
    }
    if (provider->load_merged) {
	ITERATE_SET(merged, provider->lq_index_depend_on_me, LQ_SIZE) {
        LSQ_ENTRY &lq_entry = LQ.entry.at(merged);
            lq_entry.translated = COMPLETED;
            lq_entry.physical_address = (provider->data_pa << LOG2_PAGE_SIZE) | (lq_entry.virtual_address & ((1 << LOG2_PAGE_SIZE) - 1)); // translated address
            lq_entry.event_cycle = current_core_cycle[cpu];

            while (RTL1.size() >= LQ_SIZE)
                RTL1.pop();
            RTL1.push(merged);
            DP (if (warmup_complete[cpu]) {
                    std::cout << "[RTL1] " << __func__ << " instr_id: " << lq_entry.instr_id << " rob_index: " << lq_entry.rob_index << " is added to RTL1";
                     });

            DP (if (warmup_complete[cpu]) {
                    std::cout << "[ROB] " << __func__ << " load instr_id: " << lq_entry.instr_id;
                    std::cout << " DTLB_FETCH_DONE translation: " << lq_entry.translated << std::hex << " page: " << lq_entry.physical_address>>LOG2_PAGE_SIZE;
                    std::cout << " full_addr: " << lq_entry.physical_address << std::dec << " by instr_id: " << +provider->instr_id << std::endl; });
        }
    }
}

void O3_CPU::handle_merged_load(PACKET *provider)
{
    ITERATE_SET(merged, provider->lq_index_depend_on_me, LQ_SIZE) {
        LSQ_ENTRY &lq_entry = LQ.entry.at(merged);
        ooo_model_instr &rob_entry = ROB.entry.at(lq_entry.rob_index);

        lq_entry.fetched = COMPLETED;
        lq_entry.event_cycle = current_core_cycle[cpu];
        rob_entry.num_mem_ops--;
        rob_entry.event_cycle = current_core_cycle[cpu];

#ifdef SANITY_CHECK
        if (rob_entry.num_mem_ops < 0) {
            cerr << "instr_id: " << rob_entry.instr_id << " rob_index: " << lq_entry.rob_index << endl;
            assert(0);
        }
#endif

        if (rob_entry.num_mem_ops == 0)
            inflight_mem_executions++;

        DP (if (warmup_complete[cpu]) {
        cout << "[ROB] " << __func__ << " load instr_id: " << lq_entry.instr_id;
        cout << " L1D_FETCH_DONE translation: " << +lq_entry.translated << hex << " address: " << lq_entry.physical_address>>LOG2_BLOCK_SIZE;
        cout << " full_addr: " << lq_entry.physical_address << dec << " by instr_id: " << +provider->instr_id;
        cout << " remain_mem_ops: " << rob_entry.num_mem_ops << endl; });

        release_load_queue(merged);
    }
}

void O3_CPU::release_load_queue(uint32_t lq_index)
{
    // release LQ entries
    DP ( if (warmup_complete[cpu]) {
    cout << "[LQ] " << __func__ << " instr_id: " << LQ.entry.at(lq_index).instr_id << " releases lq_index: " << lq_index;
    cout << hex << " full_addr: " << LQ.entry.at(lq_index).physical_address << dec << endl; });

    LSQ_ENTRY empty_entry;
    LQ.entry.at(lq_index) = empty_entry;
    LQ.occupancy--;
}

void O3_CPU::retire_rob()
{
    for (uint32_t n=0; n<RETIRE_WIDTH; n++) {
        ooo_model_instr &rob_entry = ROB.entry.at(ROB.head);
        if (rob_entry.ip == 0)
            return;

        // retire is in-order
        if (rob_entry.executed != COMPLETED) {
            DP ( if (warmup_complete[cpu]) {
                    std::cout << "[ROB] " << __func__ << " instr_id: " << rob_entry.instr_id << " head: " << ROB.head << " is not executed yet" << std::endl; });
            return;
        }

        // check store instruction
        uint32_t num_store = 0;
        for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
            if (rob_entry.destination_memory[i])
                num_store++;
        }

        if (num_store) {
            if ((L1D.WQ.occupancy + num_store) <= L1D.WQ.SIZE) {
                for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
                    if (rob_entry.destination_memory[i]) {

                        PACKET data_packet;
                        LSQ_ENTRY &sq_entry = SQ.entry.at(rob_entry.sq_index[i]);

                        // sq_index and rob_index are no longer available after retirement
                        // but we pass this information to avoid segmentation fault
                        data_packet.fill_level = FILL_L1;
                        data_packet.fill_l1d = 1;
                        data_packet.cpu = cpu;
                        data_packet.data_index = sq_entry.data_index;
                        data_packet.sq_index = rob_entry.sq_index[i];
                        data_packet.address = sq_entry.physical_address >> LOG2_BLOCK_SIZE;
                        data_packet.full_addr = sq_entry.physical_address;
                        data_packet.instr_id = sq_entry.instr_id;
                        data_packet.rob_index = sq_entry.rob_index;
                        data_packet.ip = sq_entry.ip;
                        data_packet.type = RFO;
                        data_packet.asid[0] = sq_entry.asid[0];
                        data_packet.asid[1] = sq_entry.asid[1];
                        data_packet.event_cycle = current_core_cycle[cpu];

                        L1D.add_wq(&data_packet);
                    }
                }
            }
            else {
                DP ( if (warmup_complete[cpu]) {
                        std::cout << "[ROB] " << __func__ << " instr_id: " << rob_entry.instr_id << " L1D WQ is full" << std::endl; });

                L1D.WQ.FULL++;
                L1D.STALL[RFO]++;

                return;
            }
        }

        // release SQ entries
        for (uint32_t i=0; i<MAX_INSTR_DESTINATIONS; i++) {
            if (rob_entry.sq_index[i] != UINT32_MAX) {
                LSQ_ENTRY &sq_entry = SQ.entry.at(rob_entry.sq_index[i]);

                DP ( if (warmup_complete[cpu]) {
                        std::cout << "[SQ] " << __func__ << " instr_id: " << rob_entry.instr_id << " releases sq_index: " << rob_entry.sq_index[i];
                        std::cout << std::hex << " address: " << (sq_entry.physical_address>>LOG2_BLOCK_SIZE);
                        std::cout << " full_addr: " << sq_entry.physical_address << std::dec << std::endl; });

                LSQ_ENTRY empty_entry;
                sq_entry = empty_entry;

                SQ.occupancy--;
                SQ.head++;
                if (SQ.head == SQ.SIZE)
                    SQ.head = 0;
            }
        }

        // release ROB entry
        DP ( if (warmup_complete[cpu]) {
                std::cout << "[ROB] " << __func__ << " instr_id: " << rob_entry.instr_id << " is retired" << std::endl; });

        ooo_model_instr empty_entry;
        rob_entry = empty_entry;

        ROB.head++;
        if (ROB.head == ROB.SIZE)
            ROB.head = 0;
        ROB.occupancy--;
        completed_executions--;
        num_retired++;
    }
}

