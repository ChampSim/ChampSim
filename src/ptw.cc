#include "ooo_cpu.h"
#include "ptw.h"
#include "vmem.h"
#include "util.h"

extern VirtualMemory vmem;
extern uint64_t current_core_cycle[NUM_CPUS];
extern uint8_t  warmup_complete[NUM_CPUS];

void PageTableWalker::handle_read()
{
    int reads_this_cycle = MAX_READ;

    while (reads_this_cycle > 0 && RQ.has_ready() && std::size(MSHR) != MSHR_SIZE)
    {
        PACKET &handle_pkt = RQ.front();

        assert((handle_pkt.full_addr >> 32) != 0xf000000f); //Page table is stored at this address
        assert(handle_pkt.full_v_addr != 0);

        PACKET packet = handle_pkt;
        packet.fill_level = FILL_L1; //This packet will be sent from L1 to PTW.
        packet.cpu = cpu;
        packet.type = TRANSLATION;
        packet.instr_id = handle_pkt.instr_id;
        packet.ip = handle_pkt.ip;
        packet.full_v_addr = handle_pkt.full_addr;
        packet.init_translation_level = 5;
        packet.full_addr = splice_bits(CR3_addr, (handle_pkt.full_addr >> get_shamt(5)) << (LOG2_PAGE_SIZE - lg2(NUM_ENTRIES_PER_PAGE)), LOG2_PAGE_SIZE);

        if (auto address_pscl5 = PSCL5.check_hit(handle_pkt.full_addr); address_pscl5 != UINT64_MAX)
        {
            packet.full_addr = splice_bits(address_pscl5, (handle_pkt.full_addr >> get_shamt(4)) << (LOG2_PAGE_SIZE - lg2(NUM_ENTRIES_PER_PAGE)), LOG2_PAGE_SIZE);
            packet.init_translation_level = 4;
        }

        if (auto address_pscl4 = PSCL4.check_hit(handle_pkt.full_addr); address_pscl4 != UINT64_MAX)
        {
            packet.full_addr = splice_bits(address_pscl4, (handle_pkt.full_addr >> get_shamt(3)) << (LOG2_PAGE_SIZE - lg2(NUM_ENTRIES_PER_PAGE)), LOG2_PAGE_SIZE);
            packet.init_translation_level = 3;
        }

        if (auto address_pscl3 = PSCL3.check_hit(handle_pkt.full_addr); address_pscl3 != UINT64_MAX)
        {
            packet.full_addr = splice_bits(address_pscl3, (handle_pkt.full_addr >> get_shamt(2)) << (LOG2_PAGE_SIZE - lg2(NUM_ENTRIES_PER_PAGE)), LOG2_PAGE_SIZE);
            packet.init_translation_level = 2;
        }

        if (auto address_pscl2 = PSCL2.check_hit(handle_pkt.full_addr); address_pscl2 != UINT64_MAX)
        {
            packet.full_addr = splice_bits(address_pscl2, (handle_pkt.full_addr >> get_shamt(1)) << (LOG2_PAGE_SIZE - lg2(NUM_ENTRIES_PER_PAGE)), LOG2_PAGE_SIZE);
            packet.init_translation_level = 1;
        }

        packet.translation_level = packet.init_translation_level;
        packet.address = packet.full_addr >> LOG2_BLOCK_SIZE;
        packet.to_return = {this};

        int rq_index = lower_level->add_rq(&packet);
        if (rq_index == -2)
            return;

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

    while (fill_this_cycle > 0 && !std::empty(MSHR) && MSHR.front().event_cycle <= current_core_cycle[cpu])
    {
        auto fill_mshr = MSHR.begin();

        std::pair key{fill_mshr->full_v_addr >> get_shamt(fill_mshr->translation_level+1), fill_mshr->translation_level};
        auto pt_it = page_table.find(key);

        if (pt_it == std::end(page_table))
        {
            if (fill_mshr->translation_level == 1)
                page_table[key] = map_data_page(fill_mshr->full_v_addr);
            else
                page_table[key] = map_translation_page(fill_mshr->full_v_addr);
        }

        if (fill_mshr->translation_level == 0) //If translation complete
        {
            for (std::size_t level = fill_mshr->init_translation_level; level > 0; --level)
            {
                std::pair fill_key{fill_mshr->full_v_addr >> get_shamt(level+1), level};

                // Check which translation levels needs to filled
                switch (level)
                {
                    case 5: PSCL5.fill_cache(page_table[fill_key], &(*fill_mshr));
                            break;
                    case 4: PSCL4.fill_cache(page_table[fill_key], &(*fill_mshr));
                            break;
                    case 3: PSCL3.fill_cache(page_table[fill_key], &(*fill_mshr));
                            break;
                    case 2: PSCL2.fill_cache(page_table[fill_key], &(*fill_mshr));
                            break;
                }
            }

            //Return the translated physical address to STLB. Does not contain last 12 bits
            fill_mshr->data      = page_table[key] >> LOG2_PAGE_SIZE;
            fill_mshr->full_addr = fill_mshr->full_v_addr;
            fill_mshr->address   = fill_mshr->full_addr >> LOG2_PAGE_SIZE;

            for (auto ret: fill_mshr->to_return)
                ret->return_data(&(*fill_mshr));

            if(warmup_complete[cpu])
                total_miss_latency += current_core_cycle[cpu] - fill_mshr->cycle_enqueued;

            MSHR.erase(fill_mshr);
        }
        else
        {
            PACKET packet = *fill_mshr;
            packet.cpu = cpu;
            packet.type = TRANSLATION;
            packet.full_addr = splice_bits(page_table[key], (fill_mshr->full_v_addr >> get_shamt(fill_mshr->translation_level)) << (LOG2_PAGE_SIZE - lg2(NUM_ENTRIES_PER_PAGE)), LOG2_PAGE_SIZE);
            packet.address = packet.full_addr >> LOG2_BLOCK_SIZE;
            packet.to_return = {this};

            int rq_index = lower_level->add_rq(&packet);
            if (rq_index != -2)
            {
                fill_mshr->event_cycle = std::numeric_limits<uint64_t>::max();
                fill_mshr->address = packet.address;
                fill_mshr->full_addr = packet.full_addr;

                MSHR.splice(std::end(MSHR), MSHR, fill_mshr);
            }
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

uint64_t PageTableWalker::map_translation_page(uint64_t full_v_addr)
{
    uint64_t physical_address = vmem.va_to_pa(cpu, next_translation_virtual_address);
    next_translation_virtual_address += PAGE_SIZE;

    return physical_address;
}

uint64_t PageTableWalker::map_data_page(uint64_t full_v_addr)
{
    return vmem.va_to_pa(cpu, full_v_addr);
}

uint64_t PageTableWalker::get_shamt(uint8_t pt_level)
{
    return LOG2_PAGE_SIZE + lg2(NUM_ENTRIES_PER_PAGE) * (pt_level-1);
}

int  PageTableWalker::add_rq(PACKET *packet)
{
    assert(packet->address != 0);

    // check for duplicates in the read queue
    auto found_rq = std::find_if(RQ.begin(), RQ.end(), eq_addr<PACKET>(packet->address));
    assert(found_rq == RQ.end()); //Duplicate request should not be sent.

    // check occupancy
    if (RQ.full()) {
        return -2; // cannot handle this request
    }

    // if there is no duplicate, add it to RQ
    RQ.push_back(*packet);

    return RQ.occupancy();
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

uint32_t PageTableWalker::get_occupancy(uint8_t queue_type, uint64_t address)
{
    if (queue_type == 0)
        return std::count_if(MSHR.begin(), MSHR.end(), is_valid<PACKET>());
    else if (queue_type == 1)
        return RQ.occupancy();
    return 0;
}

uint32_t PageTableWalker::get_size(uint8_t queue_type, uint64_t address)
{
    if (queue_type == 0)
        return MSHR_SIZE;
    else if (queue_type == 1)
        return RQ.size();
    return 0;
}

void PagingStructureCache::fill_cache(uint64_t next_level_base_addr, PACKET *packet)
{
    auto set_idx    = (packet->full_v_addr >> shamt) & bitmask(lg2(NUM_SET));
    auto set_begin  = std::next(std::begin(block), set_idx*NUM_WAY);
    auto set_end    = std::next(set_begin, NUM_WAY);
    auto fill_block = std::max_element(set_begin, set_end, lru_comparator<block_t, block_t>());

    *fill_block = {true, packet->full_v_addr, next_level_base_addr, fill_block->lru};
    std::for_each(set_begin, set_end, lru_updater<block_t>(fill_block));
}

uint64_t PagingStructureCache::check_hit(uint64_t address)
{
    auto set_idx   = (address >> shamt) & bitmask(lg2(NUM_SET));
    auto set_begin = std::next(std::begin(block), set_idx*NUM_WAY);
    auto set_end   = std::next(set_begin, NUM_WAY);
    auto hit_block = std::find_if(set_begin, set_end, eq_addr<block_t>{address, shamt});

    if (hit_block != set_end)
        return hit_block->data;

    return UINT64_MAX;
}

