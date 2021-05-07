#include "ooo_cpu.h"
#include "ptw.h"
#include "vmem.h"
#include "util.h"

extern VirtualMemory vmem;
extern uint64_t current_core_cycle[NUM_CPUS];
extern uint8_t  warmup_complete[NUM_CPUS];
extern uint8_t knob_cloudsuite;

void PageTableWalker::handle_read()
{
	int reads_this_cycle = MAX_READ;

	while(reads_this_cycle > 0)
	{
        bool mshr_full = (MSHR.size() == PTW_MSHR_SIZE);

		if(!RQ.has_ready() || mshr_full || (((CACHE*)lower_level)->RQ.occupancy() == ((CACHE*)lower_level)->RQ.size())) //PTW lower level is L1D
			break;

			PACKET &handle_pkt = RQ.front();
			
			assert((handle_pkt.full_addr >> 32) != 0xf000000f); //Page table is stored at this address
			assert(handle_pkt.full_v_addr != 0);

			uint64_t address_pscl5 = PSCL5.check_hit(handle_pkt.full_addr);
			uint64_t address_pscl4 = PSCL4.check_hit(handle_pkt.full_addr);
			uint64_t address_pscl3 = PSCL3.check_hit(handle_pkt.full_addr);
			uint64_t address_pscl2 = PSCL2.check_hit(handle_pkt.full_addr);


			PACKET packet = handle_pkt;

            packet.fill_level = FILL_L1; //This packet will be sent from L1 to PTW.
            packet.cpu = cpu;
			packet.type = TRANSLATION;
            packet.instr_id = handle_pkt.instr_id;
            packet.ip = handle_pkt.ip;
            packet.full_v_addr = handle_pkt.full_addr;

            uint64_t next_address = UINT64_MAX;

			if(address_pscl2 != UINT64_MAX)
			{
				next_address = address_pscl2 << LOG2_PAGE_SIZE | (get_offset(handle_pkt.full_addr,IS_PTL1) << 3);				
            	packet.translation_level = 1;
			}
			else if(address_pscl3 != UINT64_MAX)
			{
				next_address = address_pscl3 << LOG2_PAGE_SIZE | (get_offset(handle_pkt.full_addr,IS_PTL2) << 3);				
            	packet.translation_level = 2;
            }
			else if(address_pscl4 != UINT64_MAX)
			{
				next_address = address_pscl4 << LOG2_PAGE_SIZE | (get_offset(handle_pkt.full_addr,IS_PTL3) << 3);				
            	packet.translation_level = 3;
            }
			else if(address_pscl5 != UINT64_MAX)
			{
				next_address = address_pscl5 << LOG2_PAGE_SIZE | (get_offset(handle_pkt.full_addr,IS_PTL4) << 3);				
            	packet.translation_level = 4;
            }
            else
            {
            	next_address = CR3_addr << LOG2_PAGE_SIZE | (get_offset(handle_pkt.full_addr,IS_PTL5) << 3);				
            	packet.translation_level = 5;
            }

            packet.init_translation_level = packet.translation_level;
			packet.address = next_address >> LOG2_BLOCK_SIZE;
            packet.full_addr = next_address;

			packet.to_return.clear();
			packet.to_return = {this}; //Return this packet to PTW after completion.

			int rq_index = lower_level->add_rq(&packet);
		    assert(rq_index > -2);
			
			packet.to_return = handle_pkt.to_return; //Set the return for MSHR packet same as read packet.
			packet.type = handle_pkt.type;

            auto it = MSHR.insert(std::end(MSHR), packet);
            it->cycle_enqueued = current_core_cycle[cpu];
            it->event_cycle = std::numeric_limits<uint64_t>::max();

		    RQ.pop_front();
			reads_this_cycle--;
	}
}

void PageTableWalker::handle_fill()
{
	int fill_this_cycle = MAX_FILL;

	while(fill_this_cycle > 0) //Handle pending request
	{
		auto fill_mshr = MSHR.begin();
		if (fill_mshr == std::end(MSHR) || (fill_mshr->event_cycle > current_core_cycle[cpu])) //Check if current level translation complete
			break;

			assert(CR3_addr != UINT64_MAX);
			PageTablePage* curr_page = L5; //Start wth the L5 page
			uint64_t next_level_base_addr = UINT64_MAX;
			bool page_fault = false;

			for (int i = 5; i > fill_mshr->translation_level; i--)
			{
				uint64_t offset = get_offset(fill_mshr->full_v_addr, i); //Get offset according to page table level
				assert(curr_page != NULL);
				next_level_base_addr = curr_page->next_level_base_addr[offset];
				if(next_level_base_addr == UINT64_MAX)
				{
					handle_page_fault(curr_page, &(*fill_mshr), i); //i means next level does not exist.
					page_fault = true;
					fill_mshr->translation_level = 0; //In page fault, All levels are translated.
					break;
				}
				curr_page = curr_page->entry[offset];
			}

			if(fill_mshr->translation_level == 0) //If translation complete
			{
				curr_page = L5;
				next_level_base_addr = UINT64_MAX;
				for (int i = 5; i > 1; i--) //Walk the page table and fill MMU caches
				{
					uint64_t offset = get_offset(fill_mshr->full_v_addr, i);
					assert(curr_page != NULL);
					next_level_base_addr = curr_page->next_level_base_addr[offset];
					assert(next_level_base_addr != UINT64_MAX);
					curr_page = curr_page->entry[offset];

					if(fill_mshr->init_translation_level - i >= 0) //Check which translation levels needs to filled
					{
						switch(i)
						{
							case 5: PSCL5.fill_cache(next_level_base_addr, &(*fill_mshr));
									break;
							case 4: PSCL4.fill_cache(next_level_base_addr, &(*fill_mshr));
									break;
							case 3: PSCL3.fill_cache(next_level_base_addr, &(*fill_mshr));
									break;
							case 2: PSCL2.fill_cache(next_level_base_addr, &(*fill_mshr));
									break;
						}
					}
				}

				uint64_t offset = get_offset(fill_mshr->full_v_addr, IS_PTL1);
				next_level_base_addr = curr_page->next_level_base_addr[offset];

				fill_mshr->data = next_level_base_addr; //Return the translated physical address to STLB. Does not contain last 12 bits
		
				fill_mshr->full_addr = fill_mshr->full_v_addr;
				fill_mshr->address = fill_mshr->full_addr >> LOG2_PAGE_SIZE;
	
				for(auto ret: fill_mshr->to_return)
					ret->return_data(&(*fill_mshr));

				if(warmup_complete[cpu])
			      {
					uint64_t current_miss_latency = (current_core_cycle[cpu] - fill_mshr->cycle_enqueued);	
					total_miss_latency += current_miss_latency;
			      }

                MSHR.erase(fill_mshr);
			}
			else
			{
				assert(!page_fault); //If page fault was there, then all levels of translation should have be done.

				if((((CACHE*)lower_level)->RQ.occupancy() < ((CACHE*)lower_level)->RQ.size())) //Lower level of PTW is L1D. If L1D RQ has space then send the next level of translation.
				{
					PACKET packet = *fill_mshr;
					packet.cpu = cpu; 
					packet.type = TRANSLATION;
					packet.full_addr = next_level_base_addr << LOG2_PAGE_SIZE | (get_offset(fill_mshr->full_v_addr, fill_mshr->translation_level) << 3);
					packet.address = packet.full_addr >> LOG2_BLOCK_SIZE;
					
					packet.to_return.clear();
					packet.to_return = {this};					

                    fill_mshr->event_cycle = std::numeric_limits<uint64_t>::max();

					int rq_index = lower_level->add_rq(&packet);
					assert(rq_index > -2);

					fill_mshr->address = packet.address;
					fill_mshr->full_addr = packet.full_addr;

                    MSHR.splice(std::end(MSHR), MSHR, fill_mshr);
				}
				else
					RQ_FULL++;
			}

		fill_this_cycle--;
	}
}

void PageTableWalker::operate()
{	
	handle_fill();
	handle_read();
    RQ.operate();
}

void PageTableWalker::handle_page_fault(PageTablePage* page, PACKET *packet, uint8_t pt_level)
{
	assert(pt_level <= 5);

	while(pt_level > 1)
	{
		uint64_t offset = get_offset(packet->full_v_addr, pt_level);
		
		assert(page != NULL && page->entry[offset] == NULL);
		
		page->entry[offset] =  new PageTablePage();
		page->next_level_base_addr[offset] = map_translation_page(packet->full_v_addr, pt_level);
		write_translation_page(page->next_level_base_addr[offset], packet, pt_level);
		page = page->entry[offset];
		pt_level--;
	}

	uint64_t offset = get_offset(packet->full_v_addr, pt_level);
		
	assert(page != NULL && page->next_level_base_addr[offset] == UINT64_MAX);

	page->next_level_base_addr[offset] = map_data_page(packet->instr_id, packet->full_v_addr);
}

uint64_t PageTableWalker::map_translation_page(uint64_t full_v_addr, uint8_t pt_level)
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
	assert(packet->address != 0);

	// check for duplicates in the read queue
    auto found_rq = std::find_if(RQ.begin(), RQ.end(), eq_addr<PACKET>(packet->address));
    assert(found_rq == RQ.end()); //Duplicate request should not be sent.
    
    // check occupancy
    if (RQ.full()) {
        RQ_FULL++;
        return -2; // cannot handle this request
    }

    // if there is no duplicate, add it to RQ
    RQ.push_back(*packet);

    RQ_TO_CACHE++;
    RQ_ACCESS++;

    return RQ.occupancy();
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
    for (auto &mshr_entry : MSHR)
    {
        if (mshr_entry.address == packet->address && mshr_entry.translation_level == packet->translation_level)
        {
            assert(mshr_entry.translation_level > 0);
            mshr_entry.translation_level--;
            mshr_entry.event_cycle = current_core_cycle[cpu];

            DP (if (warmup_complete[packet->cpu]) {
                    std::cout << "[" << NAME << "_MSHR] " <<  __func__ << " instr_id: " << mshr_entry.instr_id;
                    std::cout << " address: " << std::hex << mshr_entry.address << " full_addr: " << mshr_entry.full_addr;
                    std::cout << " full_v_addr: " << mshr_entry.full_v_addr;
                    std::cout << " data: " << mshr_entry.data << std::dec;
                    std::cout << " occupancy: " << get_occupancy(0,0);
                    std::cout << " event: " << mshr_entry.event_cycle << " current: " << current_core_cycle[packet->cpu] << std::endl; });
        }
    }

    MSHR.sort(ord_event_cycle<PACKET>());
}

void PageTableWalker::increment_WQ_FULL(uint64_t address)
{
	WQ_FULL++;
}

uint32_t PageTableWalker::get_occupancy(uint8_t queue_type, uint64_t address)
{
	if (queue_type == 0)
        return std::count_if(MSHR.begin(), MSHR.end(), is_valid<PACKET>());
    else if (queue_type == 1)
        return RQ.occupancy();
    else if (queue_type == 2)
        return WQ.occupancy();
    else if (queue_type == 3)
	return PQ.occupancy();
    return 0;
}
        
uint32_t PageTableWalker::get_size(uint8_t queue_type, uint64_t address)
{
	if (queue_type == 0)
        return PTW_MSHR_SIZE;
    else if (queue_type == 1)
        return RQ.size();
    else if (queue_type == 2)
        return WQ.size();
    else if (queue_type == 3)
	return PQ.size();
    return 0;
}

uint32_t PagingStructureCache::get_set(uint64_t address)
{
    return (uint32_t) (address & ((1 << lg2(NUM_SET)) - 1)); 
}

void PagingStructureCache::fill_cache(uint64_t next_level_base_addr, PACKET *packet)
{
	uint64_t address = get_index(packet->full_v_addr);
	
    auto set_begin = std::next(std::begin(block), get_set(address)*NUM_WAY);
    auto set_end   = std::next(set_begin, NUM_WAY);
    auto fill_block = std::max_element(set_begin, set_end, lru_comparator<BLOCK, BLOCK>());

    fill_block->valid = true;
    fill_block->prefetch = (packet->type == PREFETCH);
    fill_block->dirty = false;
    fill_block->address = address;
    fill_block->full_addr = packet->full_addr;
    fill_block->v_address = packet->v_address;
    fill_block->full_v_addr = packet->full_v_addr;
    fill_block->data = next_level_base_addr;
    fill_block->ip = packet->ip;
    fill_block->cpu = packet->cpu;
    fill_block->instr_id = packet->instr_id;

    std::for_each(set_begin, set_end, lru_updater<BLOCK>(fill_block));
}

uint64_t PagingStructureCache::get_index(uint64_t address)
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

uint64_t PagingStructureCache::check_hit(uint64_t address)
{
	address = get_index(address);

	uint32_t set = get_set(address);

    if (NUM_SET < set) {
        cerr << "[" << NAME << "_ERROR] " << __func__ << " invalid set index: " << set << " NUM_SET: " << NUM_SET;
        assert(0);
    }

    for (uint32_t way=0; way < NUM_WAY; way++) {
        if (block[set * NUM_WAY + way].valid && (block[set * NUM_WAY + way].address == address)) {
	    	return block[set * NUM_WAY + way].data;
        }
    }

    return UINT64_MAX;
}
    
