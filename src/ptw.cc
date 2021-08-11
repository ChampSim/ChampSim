#include "ooo_cpu.h"
#include "ptw.h"
#include "vmem.h"
#include "util.h"

extern VirtualMemory vmem;
extern uint64_t current_core_cycle[NUM_CPUS];
extern uint8_t  warmup_complete[NUM_CPUS];

PageTableWalker::PageTableWalker(string v1, uint32_t cpu, uint32_t v2, uint32_t v3, uint32_t v4, uint32_t v5, uint32_t v6, uint32_t v7, uint32_t v8, uint32_t v9, uint32_t v10, uint32_t v11, uint32_t v12, uint32_t v13, unsigned latency)
: NAME(v1), cpu(cpu), MSHR_SIZE(v11), MAX_READ(v12), MAX_FILL(v13),
    RQ{v10, latency},
    PSCL5{"PSCL5", 4, v2, v3}, //Translation from L5->L4
    PSCL4{"PSCL4", 3, v4, v5}, //Translation from L5->L3
    PSCL3{"PSCL3", 2, v6, v7}, //Translation from L5->L2
    PSCL2{"PSCL2", 1, v8, v9}, //Translation from L5->L1
    CR3_addr(vmem.get_pte_pa(cpu, 0, vmem.pt_levels))
{
}

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
        packet.init_translation_level = vmem.pt_levels - 1;
        packet.full_addr = splice_bits(CR3_addr, vmem.get_offset(handle_pkt.full_addr, vmem.pt_levels - 1) * PTE_BYTES, LOG2_PAGE_SIZE);

        if (auto address_pscl5 = PSCL5.check_hit(handle_pkt.full_addr); address_pscl5.has_value())
        {
            packet.full_addr = address_pscl5.value();
            packet.init_translation_level = PSCL5.level - 1;
        }

        if (auto address_pscl4 = PSCL4.check_hit(handle_pkt.full_addr); address_pscl4.has_value())
        {
            packet.full_addr = address_pscl4.value();
            packet.init_translation_level = PSCL4.level - 1;
        }

        if (auto address_pscl3 = PSCL3.check_hit(handle_pkt.full_addr); address_pscl3.has_value())
        {
            packet.full_addr = address_pscl3.value();
            packet.init_translation_level = PSCL3.level - 1;
        }

        if (auto address_pscl2 = PSCL2.check_hit(handle_pkt.full_addr); address_pscl2.has_value())
        {
            packet.full_addr = address_pscl2.value();
            packet.init_translation_level = PSCL2.level - 1;
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
        if (fill_mshr->translation_level == 0) //If translation complete
        {
            // Check which translation levels needs to filled
            for (std::size_t level = fill_mshr->init_translation_level; level > 0; --level)
            {
                if (level == PSCL5.level)
                    PSCL5.fill_cache(vmem.get_pte_pa(cpu, fill_mshr->full_v_addr, PSCL5.level), &(*fill_mshr));
                if (level == PSCL4.level)
                    PSCL4.fill_cache(vmem.get_pte_pa(cpu, fill_mshr->full_v_addr, PSCL4.level), &(*fill_mshr));
                if (level == PSCL2.level)
                    PSCL3.fill_cache(vmem.get_pte_pa(cpu, fill_mshr->full_v_addr, PSCL3.level), &(*fill_mshr));
                if (level == PSCL2.level)
                    PSCL2.fill_cache(vmem.get_pte_pa(cpu, fill_mshr->full_v_addr, PSCL2.level), &(*fill_mshr));
            }

            //Return the translated physical address to STLB. Does not contain last 12 bits
            fill_mshr->data      = vmem.va_to_pa(cpu, fill_mshr->full_v_addr) >> LOG2_PAGE_SIZE;
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
            packet.full_addr = vmem.get_pte_pa(cpu, fill_mshr->full_v_addr, fill_mshr->translation_level);
            packet.address = packet.full_addr >> LOG2_BLOCK_SIZE;
            packet.to_return = {this};
            packet.translation_level = fill_mshr->translation_level - 1;

            int rq_index = lower_level->add_rq(&packet);
            if (rq_index != -2)
            {
                fill_mshr->event_cycle = std::numeric_limits<uint64_t>::max();
                fill_mshr->address = packet.address;
                fill_mshr->full_addr = packet.full_addr;
                fill_mshr->translation_level--;

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
            mshr_entry.event_cycle = current_core_cycle[cpu];

            DP (if (warmup_complete[cpu]) {
                    std::cout << "[" << NAME << "_MSHR] " <<  __func__ << " instr_id: " << mshr_entry.instr_id;
                    std::cout << " address: " << std::hex << mshr_entry.address << " full_addr: " << mshr_entry.full_addr;
                    std::cout << " full_v_addr: " << mshr_entry.full_v_addr;
                    std::cout << " data: " << mshr_entry.data << std::dec;
                    std::cout << " occupancy: " << get_occupancy(0,0);
                    std::cout << " event: " << mshr_entry.event_cycle << " current: " << current_core_cycle[cpu] << std::endl; });
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
    auto set_idx    = (packet->full_v_addr >> vmem.shamt(level+1)) & bitmask(lg2(NUM_SET));
    auto set_begin  = std::next(std::begin(block), set_idx*NUM_WAY);
    auto set_end    = std::next(set_begin, NUM_WAY);
    auto fill_block = std::max_element(set_begin, set_end, lru_comparator<block_t, block_t>());

    *fill_block = {true, packet->full_v_addr, next_level_base_addr, fill_block->lru};
    std::for_each(set_begin, set_end, lru_updater<block_t>(fill_block));
}

std::optional<uint64_t> PagingStructureCache::check_hit(uint64_t address)
{
    auto set_idx   = (address >> vmem.shamt(level+1)) & bitmask(lg2(NUM_SET));
    auto set_begin = std::next(std::begin(block), set_idx*NUM_WAY);
    auto set_end   = std::next(set_begin, NUM_WAY);
    auto hit_block = std::find_if(set_begin, set_end, eq_addr<block_t>{address, vmem.shamt(level+1)});

    if (hit_block != set_end)
        return splice_bits(hit_block->data, vmem.get_offset(address, level) * PTE_BYTES, LOG2_PAGE_SIZE);

    return {};
}

