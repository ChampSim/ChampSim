#include "cache.h"

#include <algorithm>
#include <iterator>

#include "champsim.h"
#include "champsim_constants.h"
#include "set.h"
#include "util.h"
#include "vmem.h"

#ifndef SANITY_CHECK
#define NDEBUG
#endif

uint64_t l2pf_access = 0;

extern VirtualMemory vmem;
extern uint64_t current_core_cycle[NUM_CPUS];
extern uint8_t  warmup_complete[NUM_CPUS];

class min_fill_index
{
    public:
    bool operator() (PACKET lhs, PACKET rhs)
    {
        return rhs.returned != COMPLETED || (lhs.returned == COMPLETED && lhs.event_cycle < rhs.event_cycle);
    }
};

template <>
struct is_valid<PACKET>
{
    is_valid() {}
    bool operator()(const PACKET &test)
    {
        return test.address != 0;
    }
};

void CACHE::handle_fill()
{
    while (writes_available_this_cycle > 0)
    {
        auto fill_mshr = std::min_element(std::begin(MSHR), std::end(MSHR), min_fill_index());
        if (fill_mshr->returned != COMPLETED || fill_mshr->event_cycle > current_core_cycle[fill_mshr->cpu])
            return;

        uint32_t fill_cpu = fill_mshr->cpu;

        // find victim
        uint32_t set = get_set(fill_mshr->address);
        uint32_t way = find_victim(fill_cpu, fill_mshr->instr_id, set, &block.data()[set*NUM_WAY], fill_mshr->ip, fill_mshr->full_addr, fill_mshr->type);

        bool bypass = (way == NUM_WAY);
#ifndef LLC_BYPASS
        assert(!bypass);
#endif

        BLOCK &fill_block = block[set*NUM_WAY + way];
        bool evicting_dirty = (lower_level != NULL) && fill_block.dirty;

        // In the case where we would evict a dirty block, give up and stall if the lower-level WQ is full.
        if (!bypass && evicting_dirty && (lower_level->get_occupancy(2, fill_block.address) == lower_level->get_size(2, fill_block.address)))
        {
            // lower level WQ is full, cannot replace this victim
            lower_level->increment_WQ_FULL(fill_block.address);

            DP ( if (warmup_complete[fill_cpu]) {
                    std::cout << "[" << NAME << "] " << __func__ << " stopping fill because";
                    std::cout << " lower level wq is full!" << " fill_addr: " << std::hex << fill_mshr->address;
                    std::cout << " victim_addr: " << fill_block.tag << std::dec << std::endl; });
            return;
        }

        if (!bypass)
        {
            if (evicting_dirty)
            {
                PACKET writeback_packet;

                writeback_packet.fill_level = fill_level << 1;
                writeback_packet.cpu = fill_cpu;
                writeback_packet.address = fill_block.address;
                writeback_packet.full_addr = fill_block.full_addr;
                writeback_packet.data = fill_block.data;
                writeback_packet.instr_id = fill_mshr->instr_id;
                writeback_packet.ip = 0; // writeback does not have ip
                writeback_packet.type = WRITEBACK;
                writeback_packet.event_cycle = current_core_cycle[fill_cpu];

                lower_level->add_wq(&writeback_packet);
            }

            // update prefetcher
            if (cache_type == IS_L1I)
                l1i_prefetcher_cache_fill(fill_cpu, fill_mshr->ip & ~(BLOCK_SIZE-1), set, way, fill_mshr->type == PREFETCH, fill_block.ip & ~(BLOCK_SIZE-1));
            if (cache_type == IS_L1D)
                l1d_prefetcher_cache_fill(fill_mshr->full_v_addr, fill_mshr->full_addr, set, way, fill_mshr->type == PREFETCH, fill_block.address << LOG2_BLOCK_SIZE, fill_mshr->pf_metadata);
            if (cache_type == IS_L2C)
                fill_mshr->pf_metadata = l2c_prefetcher_cache_fill(fill_mshr->v_address << LOG2_BLOCK_SIZE, fill_mshr->address << LOG2_BLOCK_SIZE, set, way, fill_mshr->type == PREFETCH, fill_block.address << LOG2_BLOCK_SIZE, fill_mshr->pf_metadata);
            if (cache_type == IS_LLC)
            {
                cpu = fill_cpu;
                fill_mshr->pf_metadata = llc_prefetcher_cache_fill(fill_mshr->v_address << LOG2_BLOCK_SIZE, fill_mshr->address << LOG2_BLOCK_SIZE, set, way, fill_mshr->type == PREFETCH, fill_block.address << LOG2_BLOCK_SIZE, fill_mshr->pf_metadata);
                cpu = 0;
            }

            // update replacement policy
            update_replacement_state(fill_cpu, set, way, fill_mshr->full_addr, fill_mshr->ip, 0, fill_mshr->type, 0);

            assert(cache_type != IS_ITLB || fill_mshr->data != 0);
            assert(cache_type != IS_DTLB || fill_mshr->data != 0);
            assert(cache_type != IS_STLB || fill_mshr->data != 0);

            if (fill_block.prefetch && !fill_block.used)
                pf_useless++;

            if (fill_mshr->type == PREFETCH)
                pf_fill++;

            auto lru = fill_block.lru; // preserve LRU state
            fill_block = *fill_mshr; // fill cache
            fill_block.lru = lru;

            // RFO marks cache line dirty
            if (fill_mshr->type == RFO && cache_type == IS_L1D)
                fill_block.dirty = 1;
        }

        if(warmup_complete[fill_cpu] && (fill_mshr->cycle_enqueued != 0))
            total_miss_latency += current_core_cycle[fill_cpu] - fill_mshr->cycle_enqueued;

        // check fill level
        if (fill_mshr->fill_level < fill_level) {
            if(fill_level == FILL_L2)
            {
                if (fill_mshr->fill_l1i)
                    upper_level_icache[fill_cpu]->return_data(&(*fill_mshr));
                if (fill_mshr->fill_l1d)
                    upper_level_dcache[fill_cpu]->return_data(&(*fill_mshr));
            }
            else
            {
                if (fill_mshr->instruction)
                    upper_level_icache[fill_cpu]->return_data(&(*fill_mshr));
                if (fill_mshr->is_data)
                    upper_level_dcache[fill_cpu]->return_data(&(*fill_mshr));
            }
        }

        // update processed packets
        if (cache_type == IS_ITLB) {
            fill_mshr->instruction_pa = fill_block.data;
            if (PROCESSED.occupancy < PROCESSED.SIZE)
                PROCESSED.add_queue(&(*fill_mshr));
        }
        else if (cache_type == IS_DTLB) {
            fill_mshr->data_pa = fill_block.data;
            if (PROCESSED.occupancy < PROCESSED.SIZE)
                PROCESSED.add_queue(&(*fill_mshr));
        }
        else if (cache_type == IS_L1I) {
            if (PROCESSED.occupancy < PROCESSED.SIZE)
                PROCESSED.add_queue(&(*fill_mshr));
        }
        else if ((cache_type == IS_L1D) && (fill_mshr->type != PREFETCH)) {
            if (PROCESSED.occupancy < PROCESSED.SIZE)
                PROCESSED.add_queue(&(*fill_mshr));
        }

        // COLLECT STATS
        sim_miss[fill_cpu][fill_mshr->type]++;
        sim_access[fill_cpu][fill_mshr->type]++;

        PACKET empty;
        *fill_mshr = empty;

        writes_available_this_cycle--;
    }
}

void CACHE::handle_writeback()
{
    while (writes_available_this_cycle > 0)
    {
        PACKET &handle_pkt = WQ.entry[WQ.head];

        // handle the oldest entry
        if ((WQ.occupancy == 0) || (handle_pkt.cpu >= NUM_CPUS) || (handle_pkt.event_cycle > current_core_cycle[handle_pkt.cpu]))
            return;

        // access cache
        uint32_t set = get_set(handle_pkt.address);
        uint32_t way = get_way(handle_pkt.address, set);

        BLOCK &fill_block = block[set*NUM_WAY + way];

        if (way < NUM_WAY) // HIT
        {
            update_replacement_state(handle_pkt.cpu, set, way, fill_block.full_addr, handle_pkt.ip, 0, handle_pkt.type, 1);

            // COLLECT STATS
            sim_hit[handle_pkt.cpu][handle_pkt.type]++;
            sim_access[handle_pkt.cpu][handle_pkt.type]++;

            // mark dirty
            fill_block.dirty = 1;

            if (cache_type == IS_ITLB)
                handle_pkt.instruction_pa = fill_block.data;
            else if (cache_type == IS_DTLB)
                handle_pkt.data_pa = fill_block.data;
            else if (cache_type == IS_STLB)
                handle_pkt.data = fill_block.data;

            // check fill level
            if (handle_pkt.fill_level < fill_level)
            {
                if(fill_level == FILL_L2)
                {
                    if (handle_pkt.fill_l1i)
                        upper_level_icache[handle_pkt.cpu]->return_data(&handle_pkt);
                    if (handle_pkt.fill_l1d)
                        upper_level_dcache[handle_pkt.cpu]->return_data(&handle_pkt);
                }
                else
                {
                    if (handle_pkt.instruction)
                        upper_level_icache[handle_pkt.cpu]->return_data(&handle_pkt);
                    if (handle_pkt.is_data)
                        upper_level_dcache[handle_pkt.cpu]->return_data(&handle_pkt);
                }
            }
        }
        else // MISS
        {
            DP ( if (warmup_complete[handle_pkt.cpu]) {
                    std::cout << "[" << NAME << "] " << __func__ << " type: " << +handle_pkt.type << " miss";
                    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << handle_pkt.address;
                    std::cout << " full_addr: " << handle_pkt.full_addr << std::dec;
                    std::cout << " cycle: " << handle_pkt.event_cycle << std::endl; });

            if (cache_type == IS_L1D) { // RFO miss

                // check mshr
                auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(handle_pkt.address));
                bool mshr_full = std::all_of(MSHR.begin(), MSHR.end(), is_valid<PACKET>());

                if (mshr_entry != MSHR.end()) // miss already inflight
                {
                    // update fill_level
                    mshr_entry->fill_level = std::min(mshr_entry->fill_level, handle_pkt.fill_level);
                    mshr_entry->fill_l1i |= handle_pkt.fill_l1i;
                    mshr_entry->fill_l1d |= handle_pkt.fill_l1d;

                    // update request
                    if (mshr_entry->type == PREFETCH) {
                        uint8_t  prior_returned = mshr_entry->returned;
                        uint64_t prior_event_cycle = mshr_entry->event_cycle;
                        *mshr_entry = handle_pkt;

                        // in case request is already returned, we should keep event_cycle and retunred variables
                        mshr_entry->returned = prior_returned;
                        mshr_entry->event_cycle = prior_event_cycle;
                    }

                    DP ( if (warmup_complete[handle_pkt.cpu]) {
                            std::cout << "[" << NAME << "] " << __func__ << " mshr merged";
                            std::cout << " instr_id: " << handle_pkt.instr_id << " prior_id: " << mshr_entry->instr_id;
                            std::cout << " address: " << std::hex << handle_pkt.address;
                            std::cout << " full_addr: " << handle_pkt.full_addr << std::dec;
                            std::cout << " cycle: " << handle_pkt.event_cycle << std::endl; });
                }
                else
                {
                    if (mshr_full) // not enough MSHR resource
                        return;

                    // check to make sure the lower level RQ has room for this RFO miss
                    if (cache_type == IS_LLC && lower_level->get_occupancy(1, handle_pkt.address) == lower_level->get_size(1, handle_pkt.address))
                        return;

                    add_mshr(&handle_pkt);
                    lower_level->add_rq(&handle_pkt);
                }
            }
            else { // Writeback miss
                // find victim
                uint32_t set = get_set(handle_pkt.address);
                uint32_t way = find_victim(handle_pkt.cpu, handle_pkt.instr_id, set, &block.data()[set*NUM_WAY], handle_pkt.ip, handle_pkt.full_addr, handle_pkt.type);

                assert(way < NUM_WAY);

                // is this dirty?
                if (fill_block.dirty && lower_level != NULL)
                {
                    if (lower_level->get_occupancy(2, fill_block.address) == lower_level->get_size(2, fill_block.address)) {

                        // lower level WQ is full, cannot replace this victim
                        lower_level->increment_WQ_FULL(fill_block.address);

                        DP ( if (warmup_complete[handle_pkt.cpu]) {
                                std::cout << "[" << NAME << "] " << __func__ << " ceasing write. ";
                                std::cout << " Lower level wq is full!" << " fill_addr: " << std::hex << handle_pkt.address;
                                std::cout << " victim_addr: " << fill_block.tag << std::dec << std::endl; });
                        return;
                    }
                    else { 
                        PACKET writeback_packet;

                        writeback_packet.fill_level = fill_level << 1;
                        writeback_packet.cpu = handle_pkt.cpu;
                        writeback_packet.address = fill_block.address;
                        writeback_packet.full_addr = fill_block.full_addr;
                        writeback_packet.data = fill_block.data;
                        writeback_packet.instr_id = handle_pkt.instr_id;
                        writeback_packet.ip = 0;
                        writeback_packet.type = WRITEBACK;
                        writeback_packet.event_cycle = current_core_cycle[handle_pkt.cpu];

                        lower_level->add_wq(&writeback_packet);
                    }
                }

                // update prefetcher
                if (cache_type == IS_L1I)
                    l1i_prefetcher_cache_fill(handle_pkt.cpu, handle_pkt.ip & ~(BLOCK_SIZE - 1), set, way, 0, fill_block.ip & ~(BLOCK_SIZE - 1));
                if (cache_type == IS_L1D)
                    l1d_prefetcher_cache_fill(handle_pkt.full_v_addr, handle_pkt.full_addr, set, way, 0, fill_block.address << LOG2_BLOCK_SIZE, handle_pkt.pf_metadata);
                else if (cache_type == IS_L2C)
                    handle_pkt.pf_metadata = l2c_prefetcher_cache_fill(handle_pkt.v_address << LOG2_BLOCK_SIZE, handle_pkt.address << LOG2_BLOCK_SIZE, set, way, 0, fill_block.address << LOG2_BLOCK_SIZE, handle_pkt.pf_metadata);
                if (cache_type == IS_LLC)
                {
                    cpu = handle_pkt.cpu;
                    handle_pkt.pf_metadata = llc_prefetcher_cache_fill(handle_pkt.v_address << LOG2_BLOCK_SIZE, handle_pkt.address << LOG2_BLOCK_SIZE, set, way, 0, fill_block.address << LOG2_BLOCK_SIZE, handle_pkt.pf_metadata);
                    cpu = 0;
                }

                // update replacement policy
                update_replacement_state(handle_pkt.cpu, set, way, handle_pkt.full_addr, handle_pkt.ip, fill_block.full_addr, handle_pkt.type, 0);

                // COLLECT STATS
                sim_miss[handle_pkt.cpu][handle_pkt.type]++;
                sim_access[handle_pkt.cpu][handle_pkt.type]++;

                assert(cache_type != IS_ITLB || handle_pkt.data != 0);
                assert(cache_type != IS_DTLB || handle_pkt.data != 0);
                assert(cache_type != IS_STLB || handle_pkt.data != 0);

                if (fill_block.prefetch && !fill_block.used)
                    pf_useless++;

                if (handle_pkt.type == PREFETCH)
                    pf_fill++;

                auto lru = fill_block.lru; // preserve LRU state
                fill_block = handle_pkt; // Fill cache
                fill_block.lru = lru;

                // mark dirty
                fill_block.dirty = 1;

                // check fill level
                if (handle_pkt.fill_level < fill_level)
                {
                    if(fill_level == FILL_L2)
                    {
                        if (handle_pkt.fill_l1i)
                            upper_level_icache[handle_pkt.cpu]->return_data(&handle_pkt);
                        if (handle_pkt.fill_l1d)
                            upper_level_dcache[handle_pkt.cpu]->return_data(&handle_pkt);
                    }
                    else
                    {
                        if (handle_pkt.instruction)
                            upper_level_icache[handle_pkt.cpu]->return_data(&handle_pkt);
                        if (handle_pkt.is_data)
                            upper_level_dcache[handle_pkt.cpu]->return_data(&handle_pkt);
                    }
                }
            }
        }

        // remove this entry from WQ
        writes_available_this_cycle--;
        WQ.remove_queue(&handle_pkt);
    }
}

void CACHE::handle_read()
{
    while (reads_available_this_cycle > 0) {

        PACKET &handle_pkt = RQ.entry[RQ.head];

        // handle the oldest entry
        if ((RQ.occupancy == 0) || (handle_pkt.cpu >= NUM_CPUS) || (handle_pkt.event_cycle > current_core_cycle[handle_pkt.cpu]))
            return;

        uint32_t set = get_set(handle_pkt.address);
        uint32_t way = get_way(handle_pkt.address, set);

        if (way < NUM_WAY) // HIT
        {
            BLOCK &hit_block = block[set*NUM_WAY + way];

            if (cache_type == IS_ITLB) {
                handle_pkt.instruction_pa = hit_block.data;
                if (PROCESSED.occupancy < PROCESSED.SIZE)
                    PROCESSED.add_queue(&handle_pkt);
            }
            else if (cache_type == IS_DTLB) {
                handle_pkt.data_pa = hit_block.data;
                if (PROCESSED.occupancy < PROCESSED.SIZE)
                    PROCESSED.add_queue(&handle_pkt);
            }
            else if (cache_type == IS_STLB) 
                handle_pkt.data = hit_block.data;
            else if (cache_type == IS_L1I) {
                if (PROCESSED.occupancy < PROCESSED.SIZE)
                    PROCESSED.add_queue(&handle_pkt);
            }
            else if ((cache_type == IS_L1D) && (RQ.entry[RQ.head].type != PREFETCH)) {
                if (PROCESSED.occupancy < PROCESSED.SIZE)
                    PROCESSED.add_queue(&handle_pkt);
            }

            // update prefetcher on load instruction
            if (handle_pkt.type == LOAD) {
                if(cache_type == IS_L1I)
                    l1i_prefetcher_cache_operate(handle_pkt.cpu, handle_pkt.ip, 1, hit_block.prefetch);
                if (cache_type == IS_L1D) 
                    l1d_prefetcher_operate(handle_pkt.full_v_addr, handle_pkt.full_addr, handle_pkt.ip, 1, handle_pkt.type);
                else if (cache_type == IS_L2C)
                    l2c_prefetcher_operate(RQ.entry[RQ.head].v_address << LOG2_BLOCK_SIZE, hit_block.address << LOG2_BLOCK_SIZE, handle_pkt.ip, 1, RQ.entry[RQ.head].type, 0);
                else if (cache_type == IS_LLC)
                {
                    cpu = handle_pkt.cpu;
                    llc_prefetcher_operate(RQ.entry[RQ.head].v_address << LOG2_BLOCK_SIZE, hit_block.address << LOG2_BLOCK_SIZE, handle_pkt.ip, 1, handle_pkt.type, 0);
                    cpu = 0;
                }
            }

            // update replacement policy
            update_replacement_state(handle_pkt.cpu, set, way, hit_block.full_addr, handle_pkt.ip, 0, handle_pkt.type, 1);

            // COLLECT STATS
            sim_hit[handle_pkt.cpu][handle_pkt.type]++;
            sim_access[handle_pkt.cpu][handle_pkt.type]++;

            // check fill level
            if (handle_pkt.fill_level < fill_level)
            {
                if(fill_level == FILL_L2)
                {
                    if (handle_pkt.fill_l1i)
                        upper_level_icache[handle_pkt.cpu]->return_data(&handle_pkt);
                    if (handle_pkt.fill_l1d)
                        upper_level_dcache[handle_pkt.cpu]->return_data(&handle_pkt);
                }
                else
                {
                    if (handle_pkt.instruction)
                        upper_level_icache[handle_pkt.cpu]->return_data(&handle_pkt);
                    if (handle_pkt.is_data)
                        upper_level_dcache[handle_pkt.cpu]->return_data(&handle_pkt);
                }
            }

            // update prefetch stats and reset prefetch bit
            if (hit_block.prefetch) {
                pf_useful++;
                hit_block.prefetch = 0;
            }
            hit_block.used = 1;
        }
        else { // read miss

            DP ( if (warmup_complete[handle_pkt.cpu]) {
                    cout << "[" << NAME << "] " << __func__ << " read miss";
                    cout << " instr_id: " << handle_pkt.instr_id << " address: " << hex << handle_pkt.address;
                    cout << " full_addr: " << handle_pkt.full_addr << dec;
                    cout << " cycle: " << handle_pkt.event_cycle << endl; });

            // check mshr
            auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(handle_pkt.address));
            bool mshr_full = std::all_of(MSHR.begin(), MSHR.end(), is_valid<PACKET>());

            if (mshr_entry != MSHR.end())
            {
                // mark merged consumer
                if (handle_pkt.type == RFO) {

                    if (handle_pkt.tlb_access) {
                        mshr_entry->sq_index_depend_on_me.insert (handle_pkt.sq_index);
                        mshr_entry->sq_index_depend_on_me.join (handle_pkt.sq_index_depend_on_me, SQ_SIZE);
                    }

                    mshr_entry->lq_index_depend_on_me.join (handle_pkt.lq_index_depend_on_me, LQ_SIZE);
                }
                else {
                    if (handle_pkt.instruction) {
                        mshr_entry->instruction = 1; // add as instruction type

                        DP (if (warmup_complete[mshr_entry->cpu]) {
                                cout << "[INSTR_MERGED] " << __func__ << " cpu: " << mshr_entry->cpu << " instr_id: " << mshr_entry->instr_id;
                                cout << " merged rob_index: " << handle_pkt.rob_index << " instr_id: " << handle_pkt.instr_id << endl; });
                    }
                    else
                    {
                        uint32_t lq_index = handle_pkt.lq_index;
                        mshr_entry->is_data = 1; // add as data type
                        mshr_entry->lq_index_depend_on_me.insert (lq_index);

                        DP (if (warmup_complete[handle_pkt.cpu]) {
                                cout << "[DATA_MERGED] " << __func__ << " cpu: " << handle_pkt.cpu << " instr_id: " << handle_pkt.instr_id;
                                cout << " merged rob_index: " << handle_pkt.rob_index << " instr_id: " << handle_pkt.instr_id << " lq_index: " << handle_pkt.lq_index << endl; });
                        mshr_entry->lq_index_depend_on_me.join (handle_pkt.lq_index_depend_on_me, LQ_SIZE);
                        mshr_entry->sq_index_depend_on_me.join (handle_pkt.sq_index_depend_on_me, SQ_SIZE);
                    }
                }

                // update fill_level
                mshr_entry->fill_level = std::min(mshr_entry->fill_level, handle_pkt.fill_level);
                mshr_entry->fill_l1i |= handle_pkt.fill_l1i;
                mshr_entry->fill_l1d |= handle_pkt.fill_l1d;

                // update request
                if (mshr_entry->type == PREFETCH) {
                    // in case request is already returned, we should keep event_cycle and retunred variables
                    handle_pkt.returned = mshr_entry->returned;
                    handle_pkt.event_cycle = mshr_entry->event_cycle;

                    *mshr_entry = handle_pkt;
                }

                DP ( if (warmup_complete[handle_pkt.cpu]) {
                        cout << "[" << NAME << "] " << __func__ << " mshr merged";
                        cout << " instr_id: " << handle_pkt.instr_id << " prior_id: " << mshr_entry->instr_id; 
                        cout << " address: " << hex << handle_pkt.address;
                        cout << " full_addr: " << handle_pkt.full_addr << dec;
                        cout << " cycle: " << handle_pkt.event_cycle << endl; });
            }
            else
            {
                if (mshr_full) // not enough MSHR resources
                    return;

                // add it to the next level's read queue
                if (lower_level)
                {
                    // check to make sure the lower level RQ has room for this read miss
                    if (cache_type == IS_LLC && lower_level->get_occupancy(1, handle_pkt.address) == lower_level->get_size(1, handle_pkt.address))
                        return;

                    add_mshr(&handle_pkt);
                    lower_level->add_rq(&handle_pkt);
                }
                else // This is the STLB
                {
                    // TODO: need to differentiate page table walk and actual swap
                    add_mshr(&handle_pkt);

                    // emulate page table walk
                    uint64_t pa = vmem.va_to_pa(handle_pkt.cpu, handle_pkt.full_addr);

                    handle_pkt.data = pa >> LOG2_PAGE_SIZE; 
                    handle_pkt.event_cycle = current_core_cycle[handle_pkt.cpu];
                    return_data(&handle_pkt);
                }
            }

            // update prefetcher on load instruction
            if (handle_pkt.type == LOAD) {
                if(cache_type == IS_L1I)
                    l1i_prefetcher_cache_operate(handle_pkt.cpu, handle_pkt.ip, 0, 0);
                if (cache_type == IS_L1D) 
                    l1d_prefetcher_operate(handle_pkt.full_v_addr, handle_pkt.full_addr, handle_pkt.ip, 0, handle_pkt.type);
                if (cache_type == IS_L2C)
                    l2c_prefetcher_operate(handle_pkt.v_address << LOG2_BLOCK_SIZE, handle_pkt.address << LOG2_BLOCK_SIZE, handle_pkt.ip, 0, handle_pkt.type, 0);
                if (cache_type == IS_LLC)
                {
                    cpu = handle_pkt.cpu;
                    llc_prefetcher_operate(handle_pkt.v_address << LOG2_BLOCK_SIZE, handle_pkt.address << LOG2_BLOCK_SIZE, handle_pkt.ip, 0, handle_pkt.type, 0);
                    cpu = 0;
                }
            }
        }

        // remove this entry from RQ
        RQ.remove_queue(&handle_pkt);
        reads_available_this_cycle--;
    }
}

void CACHE::handle_prefetch()
{
    while (reads_available_this_cycle > 0)
    {
        PACKET &handle_pkt = PQ.entry[PQ.head];

        // handle the oldest entry
        if ((PQ.occupancy == 0) || (handle_pkt.cpu >= NUM_CPUS) || (handle_pkt.event_cycle > current_core_cycle[handle_pkt.cpu]))
            return;

        uint32_t set = get_set(handle_pkt.address);
        uint32_t way = get_way(handle_pkt.address, set);

        if (way < NUM_WAY) // HIT
        {
            BLOCK &hit_block = block[set*NUM_WAY + way];

            // update replacement policy
            update_replacement_state(handle_pkt.cpu, set, way, hit_block.full_addr, handle_pkt.ip, 0, handle_pkt.type, 1);

            // COLLECT STATS
            sim_hit[handle_pkt.cpu][handle_pkt.type]++;
            sim_access[handle_pkt.cpu][handle_pkt.type]++;

            // run prefetcher on prefetches from higher caches
            if(handle_pkt.pf_origin_level < fill_level)
            {
                if (cache_type == IS_L1D)
                    l1d_prefetcher_operate(handle_pkt.full_v_addr, handle_pkt.full_addr, handle_pkt.ip, 1, PREFETCH);
                else if (cache_type == IS_L2C)
                    handle_pkt.pf_metadata = l2c_prefetcher_operate(handle_pkt.v_address << LOG2_BLOCK_SIZE, hit_block.address << LOG2_BLOCK_SIZE, handle_pkt.ip, 1, PREFETCH, handle_pkt.pf_metadata);
                else if (cache_type == IS_LLC)
                {
                    cpu = handle_pkt.cpu;
                    handle_pkt.pf_metadata = llc_prefetcher_operate(handle_pkt.v_address << LOG2_BLOCK_SIZE, hit_block.address << LOG2_BLOCK_SIZE, handle_pkt.ip, 1, PREFETCH, handle_pkt.pf_metadata);
                    cpu = 0;
                }
            }

            // check fill level
            if (handle_pkt.fill_level < fill_level)
            {
                if(fill_level == FILL_L2)
                {
                    if (handle_pkt.fill_l1i)
                        upper_level_icache[handle_pkt.cpu]->return_data(&handle_pkt);
                    if (handle_pkt.fill_l1d)
                        upper_level_dcache[handle_pkt.cpu]->return_data(&handle_pkt);
                }
                else
                {
                    if (handle_pkt.instruction)
                        upper_level_icache[handle_pkt.cpu]->return_data(&handle_pkt);
                    if (handle_pkt.is_data)
                        upper_level_dcache[handle_pkt.cpu]->return_data(&handle_pkt);
                }
            }
        }
        else { // prefetch miss

            DP ( if (warmup_complete[handle_pkt.cpu]) {
                    std::cout << "[" << NAME << "] " << __func__ << " prefetch miss";
                    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << handle_pkt.address;
                    std::cout << " full_addr: " << handle_pkt.full_addr << std::dec << " fill_level: " << handle_pkt.fill_level;
                    std::cout << " cycle: " << handle_pkt.event_cycle << std::endl; });

            // check mshr
            auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(handle_pkt.address));
            bool mshr_full = std::all_of(MSHR.begin(), MSHR.end(), is_valid<PACKET>());

            if (mshr_entry != MSHR.end())
            {
                // no need to update request except fill_level
                mshr_entry->fill_level = std::min(mshr_entry->fill_level, handle_pkt.fill_level);
                mshr_entry->fill_l1i |= handle_pkt.fill_l1i;
                mshr_entry->fill_l1d |= handle_pkt.fill_l1d;

                DP ( if (warmup_complete[handle_pkt.cpu]) {
                        std::cout << "[" << NAME << "] " << __func__ << " mshr merged";
                        std::cout << " instr_id: " << handle_pkt.instr_id << " prior_id: " << mshr_entry->instr_id; 
                        std::cout << " address: " << std::hex << handle_pkt.address;
                        std::cout << " full_addr: " << handle_pkt.full_addr << std::dec << " fill_level: " << mshr_entry->fill_level;
                        std::cout << " cycle: " << mshr_entry->event_cycle << std::endl; });
            }
            else
            {
                if (mshr_full) // Not enough MSHR resource
                    return; // TODO should we allow prefetches anyway if they will not be filled to this level?

                assert(lower_level != NULL);

                // Check lower level queue occupancy
                // LLC prefetches are reads to DRAM
                if (cache_type == IS_LLC && (lower_level->get_occupancy(1, handle_pkt.address) == lower_level->get_size(1, handle_pkt.address)))
                    return;

                // Non-LLC prefetches are prefetch requests to lower level
                if (cache_type != IS_LLC && (lower_level->get_occupancy(3, handle_pkt.address) == lower_level->get_size(3, handle_pkt.address)))
                    return;

                // run prefetcher on prefetches from higher caches
                if(handle_pkt.pf_origin_level < fill_level)
                {
                    if (cache_type == IS_L1D)
                        l1d_prefetcher_operate(handle_pkt.full_v_addr, handle_pkt.full_addr, handle_pkt.ip, 0, PREFETCH);
                    if (cache_type == IS_L2C)
                        handle_pkt.pf_metadata = l2c_prefetcher_operate(handle_pkt.v_address << LOG2_BLOCK_SIZE, handle_pkt.address << LOG2_BLOCK_SIZE, handle_pkt.ip, 0, PREFETCH, handle_pkt.pf_metadata);
                    if (cache_type == IS_LLC)
                    {
                        cpu = handle_pkt.cpu;
                        handle_pkt.pf_metadata = llc_prefetcher_operate(handle_pkt.v_address << LOG2_BLOCK_SIZE, handle_pkt.address << LOG2_BLOCK_SIZE, handle_pkt.ip, 0, PREFETCH, handle_pkt.pf_metadata);
                        cpu = 0;
                    }
                }

                // add it to MSHRs if this prefetch miss will be filled to this cache level
                if (handle_pkt.fill_level <= fill_level)
                    add_mshr(&handle_pkt);

                if (cache_type == IS_LLC)
                    lower_level->add_rq(&handle_pkt); // add it to the DRAM RQ
                else
                    lower_level->add_pq(&handle_pkt); // add it to the lower level PQ
            }

            DP ( if (warmup_complete[handle_pkt.cpu]) {
                    std::cout << "[" << NAME << "] " << __func__ << " prefetch miss handled";
                    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << handle_pkt.address;
                    std::cout << " full_addr: " << handle_pkt.full_addr << std::dec << " fill_level: " << handle_pkt.fill_level;
                    std::cout << " cycle: " << handle_pkt.event_cycle << std::endl; });
        }

        // remove this entry from PQ
        PQ.remove_queue(&handle_pkt);
        reads_available_this_cycle--;
    }
}

void CACHE::operate()
{
    // perform all writes
    writes_available_this_cycle = MAX_WRITE;
    handle_fill();
    handle_writeback();

    // perform all reads
    reads_available_this_cycle = MAX_READ;
    handle_read();

    if(VAPQ.occupancy > 0)
      {
	va_translate_prefetches();
      }

    handle_prefetch();
}

uint32_t CACHE::get_set(uint64_t address)
{
    return (uint32_t) (address & ((1 << lg2(NUM_SET)) - 1)); 
}

uint32_t CACHE::get_way(uint64_t address, uint32_t set)
{
    auto begin = std::next(block.begin(), set*NUM_WAY);
    auto end   = std::next(begin, NUM_WAY);
    return std::distance(begin, std::find_if(begin, end, eq_addr<BLOCK>(address)));
}

int CACHE::invalidate_entry(uint64_t inval_addr)
{
    uint32_t set = get_set(inval_addr);
    uint32_t way = get_way(inval_addr, set);

    if (way < NUM_WAY)
        block[set*NUM_WAY + way].valid = 0;

    return way;
}

int CACHE::add_rq(PACKET *packet)
{
    // check for the latest wirtebacks in the write queue
    int wq_index = WQ.check_queue(packet);
    if (wq_index != -1) {
        
        // check fill level
        if (packet->fill_level < fill_level) {

            packet->data = WQ.entry[wq_index].data;

            if(fill_level == FILL_L2)
            {
                if (packet->fill_l1i)
                    upper_level_icache[packet->cpu]->return_data(packet);
                if (packet->fill_l1d)
                    upper_level_dcache[packet->cpu]->return_data(packet);
            }
            else
            {
                if (packet->instruction)
                    upper_level_icache[packet->cpu]->return_data(packet);
                if (packet->is_data)
                    upper_level_dcache[packet->cpu]->return_data(packet);
            }
        }

        assert(cache_type != IS_ITLB);
        assert(cache_type != IS_DTLB);
        assert(cache_type != IS_L1I);

        // update processed packets
        if ((cache_type == IS_L1D) && (packet->type != PREFETCH)) {
            if (PROCESSED.occupancy < PROCESSED.SIZE)
                PROCESSED.add_queue(packet);

            DP ( if (warmup_complete[packet->cpu]) {
            cout << "[" << NAME << "_RQ] " << __func__ << " instr_id: " << packet->instr_id << " found recent writebacks";
            cout << hex << " read: " << packet->address << " writeback: " << WQ.entry[wq_index].address << dec;
            cout << " index: " << MAX_READ << " rob_signal: " << packet->rob_signal << endl; });
        }

        WQ.FORWARD++;
        RQ.ACCESS++;

        return -1;
    }

    // check for duplicates in the read queue
    int index = RQ.check_queue(packet);
    if (index != -1) {
        
        if (packet->instruction) {
            RQ.entry[index].instruction = 1; // add as instruction type

            DP (if (warmup_complete[packet->cpu]) {
                    std::cout << "[INSTR_MERGED] " << __func__ << " cpu: " << packet->cpu << " instr_id: " << RQ.entry[index].instr_id;
                    std::cout << " merged rob_index: " << packet->rob_index << " instr_id: " << packet->instr_id << std::endl; });
        }
        else 
        {
            // mark merged consumer
            if (packet->type == RFO) {

                uint32_t sq_index = packet->sq_index;
                RQ.entry[index].sq_index_depend_on_me.insert (sq_index);
            }
            else {
                uint32_t lq_index = packet->lq_index; 
                RQ.entry[index].lq_index_depend_on_me.insert (lq_index);

                DP (if (warmup_complete[packet->cpu]) {
                cout << "[DATA_MERGED] " << __func__ << " cpu: " << packet->cpu << " instr_id: " << RQ.entry[index].instr_id;
                cout << " merged rob_index: " << packet->rob_index << " instr_id: " << packet->instr_id << " lq_index: " << packet->lq_index << endl; });
            }
            RQ.entry[index].is_data = 1; // add as data type
        }

        RQ.entry[index].fill_l1i |= packet->fill_l1i;
        RQ.entry[index].fill_l1d |= packet->fill_l1d;

        RQ.MERGED++;
        RQ.ACCESS++;

        return index; // merged index
    }

    // check occupancy
    if (RQ.occupancy == RQ_SIZE) {
        RQ.FULL++;

        return -2; // cannot handle this request
    }

    // if there is no duplicate, add it to RQ
    index = RQ.tail;

    assert(RQ.entry[index].address == 0);

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

    DP ( if (warmup_complete[RQ.entry[index].cpu]) {
    cout << "[" << NAME << "_RQ] " <<  __func__ << " instr_id: " << RQ.entry[index].instr_id << " address: " << hex << RQ.entry[index].address;
    cout << " full_addr: " << RQ.entry[index].full_addr << dec;
    cout << " type: " << +RQ.entry[index].type << " head: " << RQ.head << " tail: " << RQ.tail << " occupancy: " << RQ.occupancy;
    cout << " event: " << RQ.entry[index].event_cycle << " current: " << current_core_cycle[RQ.entry[index].cpu] << endl; });

    if (packet->address == 0)
        assert(0);

    RQ.TO_CACHE++;
    RQ.ACCESS++;

    return -1;
}

int CACHE::add_wq(PACKET *packet)
{
    // check for duplicates in the write queue
    int index = WQ.check_queue(packet);
    if (index != -1) {

        WQ.MERGED++;
        WQ.ACCESS++;

        return index; // merged index
    }

    // sanity check
    if (WQ.occupancy >= WQ.SIZE)
        assert(0);

    // if there is no duplicate, add it to the write queue
    index = WQ.tail;
    if (WQ.entry[index].address != 0) {
        std::cerr << "[" << NAME << "_ERROR] " << __func__ << " is not empty index: " << index;
        std::cerr << " address: " << std::hex << WQ.entry[index].address;
        std::cerr << " full_addr: " << WQ.entry[index].full_addr << std::dec << std::endl;
        assert(0);
    }

    WQ.entry[index] = *packet;

    // ADD LATENCY
    if (WQ.entry[index].event_cycle < current_core_cycle[packet->cpu])
        WQ.entry[index].event_cycle = current_core_cycle[packet->cpu] + LATENCY;
    else
        WQ.entry[index].event_cycle += LATENCY;

    WQ.occupancy++;
    WQ.tail++;
    if (WQ.tail >= WQ.SIZE)
        WQ.tail = 0;

    DP (if (warmup_complete[WQ.entry[index].cpu]) {
    cout << "[" << NAME << "_WQ] " <<  __func__ << " instr_id: " << WQ.entry[index].instr_id << " address: " << hex << WQ.entry[index].address;
    cout << " full_addr: " << WQ.entry[index].full_addr << dec;
    cout << " head: " << WQ.head << " tail: " << WQ.tail << " occupancy: " << WQ.occupancy;
    cout << " data: " << hex << WQ.entry[index].data << dec;
    cout << " event: " << WQ.entry[index].event_cycle << " current: " << current_core_cycle[WQ.entry[index].cpu] << endl; });

    WQ.TO_CACHE++;
    WQ.ACCESS++;

    return -1;
}

int CACHE::prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr, int pf_fill_level, uint32_t prefetch_metadata)
{
    pf_requested++;

    if (PQ.occupancy < PQ.SIZE) {
        if ((base_addr>>LOG2_PAGE_SIZE) == (pf_addr>>LOG2_PAGE_SIZE)) {
            
            PACKET pf_packet;
            pf_packet.fill_level = pf_fill_level;
	    pf_packet.pf_origin_level = fill_level;
	    if(pf_fill_level == FILL_L1)
	      {
		pf_packet.fill_l1d = 1;
	      }
	    pf_packet.pf_metadata = prefetch_metadata;
            pf_packet.cpu = cpu;
            //pf_packet.data_index = LQ.entry[lq_index].data_index;
            //pf_packet.lq_index = lq_index;
            pf_packet.address = pf_addr >> LOG2_BLOCK_SIZE;
            pf_packet.full_addr = pf_addr;
            pf_packet.v_address = 0;
            pf_packet.full_v_addr = 0;
            //pf_packet.instr_id = LQ.entry[lq_index].instr_id;
            pf_packet.ip = ip;
            pf_packet.type = PREFETCH;
            pf_packet.event_cycle = current_core_cycle[cpu];

            // give a dummy 0 as the IP of a prefetch
            add_pq(&pf_packet);

            pf_issued++;

            return 1;
        }
    }

    return 0;
}

int CACHE::kpc_prefetch_line(uint64_t base_addr, uint64_t pf_addr, int pf_fill_level, int delta, int depth, int signature, int confidence, uint32_t prefetch_metadata)
{
    if (PQ.occupancy < PQ.SIZE) {
        if ((base_addr>>LOG2_PAGE_SIZE) == (pf_addr>>LOG2_PAGE_SIZE)) {
            
            PACKET pf_packet;
            pf_packet.fill_level = pf_fill_level;
	    pf_packet.pf_origin_level = fill_level;
	    if(pf_fill_level == FILL_L1)
              {
                pf_packet.fill_l1d = 1;
              }
	    pf_packet.pf_metadata = prefetch_metadata;
            pf_packet.cpu = cpu;
            //pf_packet.data_index = LQ.entry[lq_index].data_index;
            //pf_packet.lq_index = lq_index;
            pf_packet.address = pf_addr >> LOG2_BLOCK_SIZE;
            pf_packet.full_addr = pf_addr;
            pf_packet.v_address = 0;
            pf_packet.full_v_addr = 0;
            //pf_packet.instr_id = LQ.entry[lq_index].instr_id;
            pf_packet.ip = 0;
            pf_packet.type = PREFETCH;
            pf_packet.delta = delta;
            pf_packet.depth = depth;
            pf_packet.signature = signature;
            pf_packet.confidence = confidence;
            pf_packet.event_cycle = current_core_cycle[cpu];

            // give a dummy 0 as the IP of a prefetch
            add_pq(&pf_packet);

            pf_issued++;

            return 1;
        }
    }

    return 0;
}

int CACHE::va_prefetch_line(uint64_t ip, uint64_t pf_addr, int pf_fill_level, uint32_t prefetch_metadata)
{
  if(pf_addr == 0)
  {
      std::cerr << "va_prefetch_line() pf_addr cannot be 0! exiting" << std::endl;
      assert(0);
  }

  pf_requested++;
  if(VAPQ.occupancy < VAPQ.SIZE)
    {
      // generate new prefetch request packet
      PACKET pf_packet;
      pf_packet.fill_level = pf_fill_level;
      pf_packet.pf_origin_level = fill_level;
      if(pf_fill_level == FILL_L1)
	{
	  pf_packet.fill_l1d = 1;
	}
      pf_packet.pf_metadata = prefetch_metadata;
      pf_packet.cpu = cpu;
      pf_packet.v_address = pf_addr >> LOG2_BLOCK_SIZE;
      pf_packet.address = pf_addr >> LOG2_BLOCK_SIZE; // make address == v_address before translation just so we can use VAPQ's check_queue() function
	pf_packet.full_v_addr = pf_addr;
      pf_packet.full_addr = pf_addr;
      pf_packet.ip = ip;
      pf_packet.type = PREFETCH;
      pf_packet.event_cycle = 0;

      int vapq_index = VAPQ.check_queue(&pf_packet);
      if(vapq_index != -1)
	{
	  // there's already a VA prefetch to this cache line
	  return 1;
	}

      // add the packet to the virtual address space prefetching queue
      int index = VAPQ.tail;
      VAPQ.entry[index] = pf_packet;
      VAPQ.occupancy++;
      VAPQ.tail++;
      if (VAPQ.tail >= VAPQ.SIZE)
	{
	  VAPQ.tail = 0;
	}

      return 1;
    }

  return 0;
}

void CACHE::va_translate_prefetches()
{
  // move translated prefetches from the VAPQ to the regular PQ
  uint32_t vapq_index = VAPQ.head;
  if (PQ.occupancy < PQ.SIZE)
    {
      for(uint32_t i=0; i<VAPQ.SIZE; i++)
	{
	  // identify a VA prefetch that is fully translated
	  if((VAPQ.entry[vapq_index].address != 0) && (VAPQ.entry[vapq_index].address != VAPQ.entry[vapq_index].v_address))
	    {
	      // move the translated prefetch over to the regular PQ
	      add_pq(&VAPQ.entry[vapq_index]);

	      // remove the prefetch from the VAPQ
	      VAPQ.remove_queue(&VAPQ.entry[vapq_index]);

	      break;
	    }
	  vapq_index++;
	  if(vapq_index >= VAPQ.SIZE)
	    {
	      vapq_index = 0;
	    }
	}
    }

  // TEMPORARY SOLUTION: mark prefetches as translated after a fixed latency
  vapq_index = VAPQ.head;
  for(uint32_t i=0; i<VAPQ.SIZE; i++)
    {
      if((VAPQ.entry[vapq_index].address == VAPQ.entry[vapq_index].v_address) && (VAPQ.entry[vapq_index].event_cycle <= current_core_cycle[cpu]))
        {
	  VAPQ.entry[vapq_index].full_addr = vmem.va_to_pa(cpu, VAPQ.entry[vapq_index].full_v_addr);
	  VAPQ.entry[vapq_index].address = (VAPQ.entry[vapq_index].full_addr)>>LOG2_BLOCK_SIZE;
          break;
        }
      vapq_index++;
      if(vapq_index >= VAPQ.SIZE)
        {
          vapq_index = 0;
        }
    }

  // initiate translation of new items in VAPQ
  vapq_index = VAPQ.head;
  for(uint32_t i=0; i<VAPQ.SIZE; i++)
    {
      if((VAPQ.entry[vapq_index].address == VAPQ.entry[vapq_index].v_address) && (VAPQ.entry[vapq_index].event_cycle == 0))
	{
	  VAPQ.entry[vapq_index].event_cycle = current_core_cycle[cpu] + VA_PREFETCH_TRANSLATION_LATENCY;
	  break;
	}
      vapq_index++;
      if(vapq_index >= VAPQ.SIZE)
	{
	  vapq_index = 0;
	}
    }
}

int CACHE::add_pq(PACKET *packet)
{
    // check for the latest wirtebacks in the write queue
    int wq_index = WQ.check_queue(packet);
    if (wq_index != -1) {
        
        // check fill level
        if (packet->fill_level < fill_level) {

            packet->data = WQ.entry[wq_index].data;

            if(fill_level == FILL_L2)
            {
                if (packet->fill_l1i)
                    upper_level_icache[packet->cpu]->return_data(packet);
                if (packet->fill_l1d)
                    upper_level_dcache[packet->cpu]->return_data(packet);
            }
            else
            {
                if (packet->instruction)
                    upper_level_icache[packet->cpu]->return_data(packet);
                if (packet->is_data)
                    upper_level_dcache[packet->cpu]->return_data(packet);
            }
        }

        WQ.FORWARD++;
        PQ.ACCESS++;

        return -1;
    }

    // check for duplicates in the PQ
    int index = PQ.check_queue(packet);
    if (index != -1)
    {
        PQ.entry[index].fill_level   = std::min(PQ.entry[index].fill_level, packet->fill_level);
        PQ.entry[index].instruction |= packet->instruction;
        PQ.entry[index].is_data     |= packet->is_data;
        PQ.entry[index].fill_l1i    |= packet->fill_l1i;
        PQ.entry[index].fill_l1d    |= packet->fill_l1d;

        PQ.MERGED++;
        PQ.ACCESS++;

        return index; // merged index
    }

    // check occupancy
    if (PQ.occupancy == PQ_SIZE) {
        PQ.FULL++;

        DP ( if (warmup_complete[packet->cpu]) {
        cout << "[" << NAME << "] cannot process add_pq since it is full" << endl; });
        return -2; // cannot handle this request
    }

    // if there is no duplicate, add it to PQ
    index = PQ.tail;

    assert(PQ.entry[index].address == 0);

    PQ.entry[index] = *packet;

    // ADD LATENCY
    if (PQ.entry[index].event_cycle < current_core_cycle[packet->cpu])
        PQ.entry[index].event_cycle = current_core_cycle[packet->cpu] + LATENCY;
    else
        PQ.entry[index].event_cycle += LATENCY;

    PQ.occupancy++;
    PQ.tail++;
    if (PQ.tail >= PQ.SIZE)
        PQ.tail = 0;

    DP ( if (warmup_complete[PQ.entry[index].cpu]) {
    cout << "[" << NAME << "_PQ] " <<  __func__ << " instr_id: " << PQ.entry[index].instr_id << " address: " << hex << PQ.entry[index].address;
    cout << " full_addr: " << PQ.entry[index].full_addr << dec;
    cout << " type: " << +PQ.entry[index].type << " head: " << PQ.head << " tail: " << PQ.tail << " occupancy: " << PQ.occupancy;
    cout << " event: " << PQ.entry[index].event_cycle << " current: " << current_core_cycle[PQ.entry[index].cpu] << endl; });

    if (packet->address == 0)
        assert(0);

    PQ.TO_CACHE++;
    PQ.ACCESS++;

    return -1;
}

void CACHE::return_data(PACKET *packet)
{
    // check MSHR information
    auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(packet->address));

    // sanity check
    if (mshr_entry == MSHR.end()) {
        std::cerr << "[" << NAME << "_MSHR] " << __func__ << " instr_id: " << packet->instr_id << " cannot find a matching entry!";
        std::cerr << " full_addr: " << std::hex << packet->full_addr;
        std::cerr << " address: " << packet->address << std::dec;
        std::cerr << " event: " << packet->event_cycle << " current: " << current_core_cycle[packet->cpu] << std::endl;
        assert(0);
    }

    // MSHR holds the most updated information about this request
    // no need to do memcpy
    mshr_entry->returned = COMPLETED;
    mshr_entry->data = packet->data;
    mshr_entry->pf_metadata = packet->pf_metadata;

    // ADD LATENCY
    if (mshr_entry->event_cycle < current_core_cycle[packet->cpu])
        mshr_entry->event_cycle = current_core_cycle[packet->cpu] + LATENCY;
    else
        mshr_entry->event_cycle += LATENCY;

    DP (if (warmup_complete[packet->cpu]) {
            std::cout << "[" << NAME << "_MSHR] " <<  __func__ << " instr_id: " << mshr_entry->instr_id;
            std::cout << " address: " << std::hex << mshr_entry->address << " full_addr: " << mshr_entry->full_addr;
            std::cout << " data: " << mshr_entry->data << std::dec;
            std::cout << " index: " << std::distance(MSHR.begin(), mshr_entry) << " occupancy: " << get_occupancy(0,0);
            std::cout << " event: " << mshr_entry->event_cycle << " current: " << current_core_cycle[packet->cpu] << std::endl; });
}

void CACHE::add_mshr(PACKET *packet)
{
    auto it = std::find_if_not(MSHR.begin(), MSHR.end(), is_valid<PACKET>());
    if (it != MSHR.end())
    {
        *it = *packet;
        it->returned = INFLIGHT;
        it->cycle_enqueued = current_core_cycle[packet->cpu];
    }
}

uint32_t CACHE::get_occupancy(uint8_t queue_type, uint64_t address)
{
    if (queue_type == 0)
        return std::count_if(MSHR.begin(), MSHR.end(), is_valid<PACKET>());
    else if (queue_type == 1)
        return RQ.occupancy;
    else if (queue_type == 2)
        return WQ.occupancy;
    else if (queue_type == 3)
        return PQ.occupancy;

    return 0;
}

uint32_t CACHE::get_size(uint8_t queue_type, uint64_t address)
{
    if (queue_type == 0)
        return MSHR_SIZE;
    else if (queue_type == 1)
        return RQ.SIZE;
    else if (queue_type == 2)
        return WQ.SIZE;
    else if (queue_type == 3)
        return PQ.SIZE;

    return 0;
}

void CACHE::increment_WQ_FULL(uint64_t address)
{
    WQ.FULL++;
}
