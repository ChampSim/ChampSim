#include "ooo_cpu.h"
#include "ptw.h"
#include "vmem.h"

extern VirtualMemory vmem;
extern uint64_t current_core_cycle[NUM_CPUS];
extern uint8_t  warmup_complete[NUM_CPUS];
extern uint8_t knob_cloudsuite;

void PageTableWalker::operate()
{

#if !defined(INSERT_PAGE_TABLE_WALKER)
	assert(0);
#endif

	if(MSHR.occupancy > 0) //Handle pending request, only one request is serviced at a time.
	{
		if((MSHR.entry[MSHR.head].returned == COMPLETED) && (MSHR.entry[MSHR.head].event_cycle <= current_core_cycle[cpu])) //Check if current level translation complete
		{
			int index = MSHR.head;

			assert(CR3_addr != UINT64_MAX);
			PageTablePage* curr_page = L5; //Start wth the L5 page
			uint64_t next_level_base_addr = UINT64_MAX;
			bool page_fault = false;

			for (int i = 5; i > MSHR.entry[index].translation_level; i--)
			{
				uint64_t offset = get_offset(MSHR.entry[index].full_v_addr, i); //Get offset according to page table level
				assert(curr_page != NULL);
				next_level_base_addr = curr_page->next_level_base_addr[offset];
				if(next_level_base_addr == UINT64_MAX)
				{
					handle_page_fault(curr_page, &MSHR.entry[index], i); //i means next level does not exist.
					page_fault = true;
					MSHR.entry[index].translation_level = 0; //In page fault, All levels are translated.
					break;
				}
				curr_page = curr_page->entry[offset];
			}

			if(MSHR.entry[index].translation_level == 0) //If translation complete
			{
				curr_page = L5;
				next_level_base_addr = UINT64_MAX;
				for (int i = 5; i > 1; i--) //Walk the page table and fill MMU caches
				{
					uint64_t offset = get_offset(MSHR.entry[index].full_v_addr, i);
					assert(curr_page != NULL);
					next_level_base_addr = curr_page->next_level_base_addr[offset];
					assert(next_level_base_addr != UINT64_MAX);
					curr_page = curr_page->entry[offset];

					if(MSHR.entry[index].init_translation_level - i >= 0) //Check which translation levels needs to filled
					{
						switch(i)
						{
							case 5: fill_mmu_cache(PSCL5, next_level_base_addr, &MSHR.entry[index], IS_PSCL5);
									break;
							case 4: fill_mmu_cache(PSCL4, next_level_base_addr, &MSHR.entry[index], IS_PSCL4);
									break;
							case 3: fill_mmu_cache(PSCL3, next_level_base_addr, &MSHR.entry[index], IS_PSCL3);
									break;
							case 2: fill_mmu_cache(PSCL2, next_level_base_addr, &MSHR.entry[index], IS_PSCL2);
									break;
						}
					}
				}

				uint64_t offset = get_offset(MSHR.entry[index].full_v_addr, IS_PTL1);
				next_level_base_addr = curr_page->next_level_base_addr[offset];

				MSHR.entry[index].event_cycle = current_core_cycle[cpu]; //Page fault are completed in same cycle	


				MSHR.entry[index].data = next_level_base_addr << LOG2_PAGE_SIZE | (MSHR.entry[index].full_v_addr & ((1<<LOG2_PAGE_SIZE) - 1)); //Return the translated physical address to STLB
			
				for(auto ret: MSHR.entry[index].to_return)
					ret->return_data(&MSHR.entry[index]);

				if(warmup_complete[cpu])
			      {
					uint64_t current_miss_latency = (current_core_cycle[cpu] - MSHR.entry[index].cycle_enqueued);	
					total_miss_latency += current_miss_latency;
			      }

				MSHR.remove_queue(&MSHR.entry[index]);
			}
			else
			{
				assert(!page_fault); //If page fault was there, then all levels of translation should have be done.

				if((((CACHE*)lower_level)->RQ.occupancy < ((CACHE*)lower_level)->RQ.SIZE)) //Lower level of PTW is L2C. If L2 RQ has space then send the next level of translation.
				{
					PACKET packet = MSHR.entry[index];
					packet.cpu = cpu; 
					packet.type = TRANSLATION;
					packet.event_cycle = current_core_cycle[cpu];
					packet.full_addr = next_level_base_addr << LOG2_PAGE_SIZE | (get_offset(MSHR.entry[index].full_v_addr, MSHR.entry[index].translation_level) << 3);
					packet.address = MSHR.entry[index].full_addr >> LOG2_BLOCK_SIZE;
					
					packet.to_return.clear();
					packet.to_return = {this};					

					MSHR.entry[index].returned = INFLIGHT;

					int rq_index = lower_level->add_rq(&packet);
					assert(rq_index == -1); //Since a single request is processed at a time, translation packet cannot merge in RQ.
				}
				else
					rq_full++;
			}
		}
	}
	else if(RQ.occupancy > 0) //If there is no pending request which is undergoing translation, then process new request.
	{
		if((RQ.entry[RQ.head].event_cycle <= current_core_cycle[cpu]) && (((CACHE*)lower_level)->RQ.occupancy < ((CACHE*)lower_level)->RQ.SIZE)) //PTW lower level is L2C.
		{
			int index = RQ.head;
			
			assert((RQ.entry[index].full_addr >> 32) != 0xf000000f); //Page table is stored at this address
			assert(RQ.entry[index].full_v_addr != 0);

			uint64_t address_pscl5 = check_hit(PSCL5,get_index(RQ.entry[index].full_addr,IS_PSCL5),RQ.entry[index].type);
			uint64_t address_pscl4 = check_hit(PSCL4,get_index(RQ.entry[index].full_addr,IS_PSCL4),RQ.entry[index].type);
			uint64_t address_pscl3 = check_hit(PSCL3,get_index(RQ.entry[index].full_addr,IS_PSCL3),RQ.entry[index].type);
			uint64_t address_pscl2 = check_hit(PSCL2,get_index(RQ.entry[index].full_addr,IS_PSCL2),RQ.entry[index].type);


			PACKET packet = RQ.entry[index];

            packet.fill_level = FILL_L1; //This packet will be sent from L2 to PTW.
            packet.cpu = cpu;
			packet.type = TRANSLATION;
            packet.instr_id = RQ.entry[index].instr_id;
            packet.ip = RQ.entry[index].ip;
            packet.event_cycle = current_core_cycle[cpu];
            packet.full_v_addr = RQ.entry[index].full_addr;

            uint64_t next_address = UINT64_MAX;

			if(address_pscl2 != UINT64_MAX)
			{
				next_address = address_pscl2 << LOG2_PAGE_SIZE | (get_offset(RQ.entry[index].full_addr,IS_PTL1) << 3);				
            	packet.translation_level = 1;
			}
			else if(address_pscl3 != UINT64_MAX)
			{
				next_address = address_pscl3 << LOG2_PAGE_SIZE | (get_offset(RQ.entry[index].full_addr,IS_PTL2) << 3);				
            	packet.translation_level = 2;
            }
			else if(address_pscl4 != UINT64_MAX)
			{
				next_address = address_pscl4 << LOG2_PAGE_SIZE | (get_offset(RQ.entry[index].full_addr,IS_PTL3) << 3);				
            	packet.translation_level = 3;
            }
			else if(address_pscl5 != UINT64_MAX)
			{
				next_address = address_pscl5 << LOG2_PAGE_SIZE | (get_offset(RQ.entry[index].full_addr,IS_PTL4) << 3);				
            	packet.translation_level = 4;
            }
            else
            {
            	if(CR3_addr == UINT64_MAX)
            	{
            		assert(!CR3_set); //This should be called only once when the process is starting
            		handle_page_fault(L5, &RQ.entry[index], 6); //6 means first level is also not there
            		CR3_set = true;

            		PageTablePage* curr_page = L5;
					uint64_t next_level_base_addr = UINT64_MAX;
					for (int i = 5; i > 1; i--) //Fill MMU caches
					{
						uint64_t offset = get_offset(RQ.entry[index].full_v_addr, i);
						assert(curr_page != NULL);
						next_level_base_addr = curr_page->next_level_base_addr[offset];
						assert(next_level_base_addr != UINT64_MAX); //Page fault serviced, all levels should be there.
						curr_page = curr_page->entry[offset];

						switch(i)
						{
							case 5: fill_mmu_cache(PSCL5, next_level_base_addr, &RQ.entry[index], IS_PSCL5);
									break;
							case 4: fill_mmu_cache(PSCL4, next_level_base_addr, &RQ.entry[index], IS_PSCL4);
									break;
							case 3: fill_mmu_cache(PSCL3, next_level_base_addr, &RQ.entry[index], IS_PSCL3);
									break;
							case 2: fill_mmu_cache(PSCL2, next_level_base_addr, &RQ.entry[index], IS_PSCL2);
									break;
						}
					}

					uint64_t offset = get_offset(RQ.entry[index].full_v_addr, IS_PTL1);
					next_level_base_addr = curr_page->next_level_base_addr[offset];

					RQ.entry[index].event_cycle = current_core_cycle[cpu]; //No penalty for page table setup
					RQ.entry[index].data = next_level_base_addr << LOG2_PAGE_SIZE | (RQ.entry[index].full_v_addr & ((1<<LOG2_PAGE_SIZE) - 1));
				
	
					for(auto ret: RQ.entry[index].to_return)
						ret->return_data(&RQ.entry[index]);

					RQ.remove_queue(&RQ.entry[index]);

					return;

            	}
            	next_address = CR3_addr << LOG2_PAGE_SIZE | (get_offset(RQ.entry[index].full_addr,IS_PTL5) << 3);				
            	packet.translation_level = 5;
            }

            packet.init_translation_level = packet.translation_level;
			packet.address = next_address >> LOG2_BLOCK_SIZE;
            packet.full_addr = next_address;

			packet.to_return.clear();
			packet.to_return = {this}; //Return this packet to PTW after completion.

			int rq_index = lower_level->add_rq(&packet);
		    assert(rq_index == -1); //Packet should not merge as one translation is sent at a time.
			
			packet.to_return = RQ.entry[index].to_return; //Set the return for MSHR packet same as read packet.` 
		    packet.address = RQ.entry[index].address;
			packet.full_addr = RQ.entry[index].full_addr;
			packet.type = RQ.entry[index].type;
			add_mshr(&packet);

		    RQ.remove_queue(&RQ.entry[index]);
		}
	}

}

void PageTableWalker::handle_page_fault(PageTablePage* page, PACKET *packet, uint8_t pt_level)
{
	if(pt_level == 6)
	{
		assert(page == NULL && CR3_addr == UINT64_MAX);
		L5 = new PageTablePage();
		CR3_addr = map_translation_page();
		pt_level--;
		write_translation_page(CR3_addr, packet, pt_level);
		page = L5;
	}

	while(pt_level > 1)
	{
		uint64_t offset = get_offset(packet->full_v_addr, pt_level);
		
		assert(page != NULL && page->entry[offset] == NULL);
		
		page->entry[offset] =  new PageTablePage();
		page->next_level_base_addr[offset] = map_translation_page();
		write_translation_page(page->next_level_base_addr[offset], packet, pt_level);
		page = page->entry[offset];
		pt_level--;
	}

	uint64_t offset = get_offset(packet->full_v_addr, pt_level);
		
	assert(page != NULL && page->next_level_base_addr[offset] == UINT64_MAX);

	page->next_level_base_addr[offset] = map_data_page(packet->instr_id, packet->full_v_addr);
}

uint64_t PageTableWalker::map_translation_page()
{
	uint64_t physical_address = vmem.va_to_pa(cpu, next_translation_virtual_address);
	next_translation_virtual_address = ( (next_translation_virtual_address >> LOG2_PAGE_SIZE) + 1 ) << LOG2_PAGE_SIZE;
	
	return physical_address >> LOG2_PAGE_SIZE;
}

uint64_t PageTableWalker::map_data_page(uint64_t instr_id, uint64_t full_v_addr)
{
	uint64_t physical_address = vmem.va_to_pa(cpu, full_v_addr);
    return physical_address >> LOG2_PAGE_SIZE;
}

void PageTableWalker::write_translation_page(uint64_t next_level_base_addr, PACKET *packet, uint8_t pt_level)
{
}

void PageTableWalker::add_mshr(PACKET *packet)
{
	uint32_t index = 0; //One request is processed at a time

    packet->cycle_enqueued = current_core_cycle[packet->cpu];

    MSHR.entry[index] = *packet;
    MSHR.entry[index].returned = INFLIGHT;
    MSHR.occupancy++;
}

void PageTableWalker::fill_mmu_cache(CACHE &cache, uint64_t next_level_base_addr, PACKET *packet, uint8_t cache_type)
{
	uint64_t address = get_index(packet->full_v_addr, cache_type);
	
	uint32_t set = cache.get_set(address);
	uint32_t way = cache.find_victim(packet->cpu, packet->instr_id, set, &cache.block.data()[set*cache.NUM_WAY], packet->ip, packet->full_addr, packet->type);

	PACKET new_packet = *packet;
	
	new_packet.address = address;
	new_packet.data = next_level_base_addr;

	BLOCK &fill_block = cache.block[set * cache.NUM_WAY + way];
	
	auto lru = fill_block.lru;	
	fill_block = new_packet;	
	fill_block.lru = lru;		
	
	 cache.update_replacement_state(packet->cpu, set, way, packet->full_addr, packet->ip, 0, packet->type, 0);

	 cache.sim_miss[packet->cpu][packet->type]++;
     cache.sim_access[packet->cpu][packet->type]++;
     
}

uint64_t PageTableWalker::get_index(uint64_t address, uint8_t cache_type)
{

	address = address & ( (1L<<57) -1); //Extract Last 57 bits

	int shift = 12;

	switch(cache_type)
	{
		case IS_PSCL5: shift+= 9+9+9+9;
					   break;
		case IS_PSCL4: shift+= 9+9+9;
					   break;
		case IS_PSCL3: shift+= 9+9;
					   break;
		case IS_PSCL2: shift+= 9; //Most siginificant 36 bits will be used to index PSCL2 
					   break;
	}

	return (address >> shift); 
}

uint64_t PageTableWalker::check_hit(CACHE &cache, uint64_t address, uint8_t type)
{

	uint32_t set = cache.get_set(address);

    if (cache.NUM_SET < set) {
        cerr << "[" << NAME << "_ERROR] " << __func__ << " invalid set index: " << set << " NUM_SET: " << cache.NUM_SET;
        assert(0);
    }

    for (uint32_t way=0; way<cache.NUM_WAY; way++) {
        if (cache.block[set * cache.NUM_WAY + way].valid && (cache.block[set * cache.NUM_WAY + way].tag == address)) {
	    
	    	// COLLECT STATS
            cache.sim_hit[cpu][type]++;
            cache.sim_access[cpu][type]++;
     
	    return cache.block[set *cache.NUM_WAY + way].data;
        }
    }

    return UINT64_MAX;
}
    
uint64_t PageTableWalker::get_offset(uint64_t full_virtual_addr, uint8_t pt_level)
{
	full_virtual_addr = full_virtual_addr & ( (1L<<57) -1); //Extract Last 57 bits

	int shift = 12;

	switch(pt_level)
	{
		case IS_PTL5: shift+= 9+9+9+9;
					   break;
		case IS_PTL4: shift+= 9+9+9;
					   break;
		case IS_PTL3: shift+= 9+9;
					   break;
	   	case IS_PTL2: shift+= 9;
	   				   break;
	}

	uint64_t offset = (full_virtual_addr >> shift) & 0x1ff; //Extract the offset to generate next physical address

	return offset; 
}

int  PageTableWalker::add_rq(PACKET *packet)
{
	// check for duplicates in the read queue
    int index = RQ.check_queue(packet);
    assert(index == -1); //Duplicate request should not be sent.
    
    // check occupancy
    if (RQ.occupancy == PTW_RQ_SIZE) {
        RQ.FULL++;

        return -2; // cannot handle this request
    }

    // if there is no duplicate, add it to RQ
    index = RQ.tail;

#ifdef SANITY_CHECK
    if (RQ.entry[index].address != 0) {
        cerr << "[" << NAME << "_ERROR] " << __func__ << " is not empty index: " << index;
        cerr << " address: " << hex << RQ.entry[index].address;
        cerr << " full_addr: " << RQ.entry[index].full_addr << dec << endl;
        assert(0);
    }
#endif

    RQ.entry[index] = *packet;

    // ADD LATENCY
    if (RQ.entry[index].event_cycle < current_core_cycle[packet->cpu])
        RQ.entry[index].event_cycle = current_core_cycle[packet->cpu] + LATENCY;
    else
        RQ.entry[index].event_cycle += LATENCY;

    RQ.occupancy++;
    RQ.tail++;
    if (RQ.tail >= RQ.SIZE)
        RQ.tail = 0;

    if (packet->address == 0)
        assert(0);

    RQ.TO_CACHE++;
    RQ.ACCESS++;

    return -1;
}

int PageTableWalker::add_wq(PACKET *packet)
{
	assert(0); //No request is added to WQ
}

int PageTableWalker::add_pq(PACKET *packet)
{
	assert(0); //No request is added to PQ
}

void PageTableWalker::return_data(PACKET *packet)
{

	int mshr_index = -1;
	
	// search MSHR
    for (uint32_t index=0; index < MSHR.SIZE; index++) {
		if (MSHR.entry[index].full_v_addr == packet->full_v_addr) {
		    mshr_index = index;
		    break;
		}
    }

    // sanity check
    if (mshr_index == -1) {
        cerr << "[" << NAME << "_MSHR] " << __func__ << " instr_id: " << packet->instr_id << " cannot find a matching entry!";
        cerr << " full_addr: " << hex << packet->full_addr;
        cerr << " address: " << packet->address << dec;
        cerr << " event: " << packet->event_cycle << " current: " << current_core_cycle[packet->cpu] << endl;
        assert(0);
    }

    // MSHR holds the most updated information about this request
    // no need to do memcpy
    MSHR.num_returned++;
    MSHR.entry[mshr_index].returned = COMPLETED;

    assert(MSHR.entry[mshr_index].translation_level > 0);
    MSHR.entry[mshr_index].translation_level--;
}

void PageTableWalker::increment_WQ_FULL(uint64_t address)
{
	WQ.FULL++;
}

uint32_t PageTableWalker::get_occupancy(uint8_t queue_type, uint64_t address)
{
	if (queue_type == 0)
        return MSHR.occupancy;
    else if (queue_type == 1)
        return RQ.occupancy;
    else if (queue_type == 2)
        return WQ.occupancy;
    else if (queue_type == 3)
	return PQ.occupancy;
    return 0;
}
        
uint32_t PageTableWalker::get_size(uint8_t queue_type, uint64_t address)
{
	if (queue_type == 0)
        return MSHR.SIZE;
    else if (queue_type == 1)
        return RQ.SIZE;
    else if (queue_type == 2)
        return WQ.SIZE;
    else if (queue_type == 3)
	return PQ.SIZE;
    return 0;
}
