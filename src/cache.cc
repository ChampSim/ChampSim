#include "cache.h"

#include <algorithm>
#include <iterator>

#include "champsim.h"
#include "set.h"
#include "vmem.h"

#ifndef SANITY_CHECK
#define NDEBUG
#endif

uint64_t l2pf_access = 0;

extern VirtualMemory vmem;

class min_fill_index
{
    public:
    bool operator() (PACKET lhs, PACKET rhs)
    {
        return rhs.returned != COMPLETED || (lhs.returned == COMPLETED && lhs.event_cycle < rhs.event_cycle);
    }
};

template <typename T>
struct eq_addr
{
    const decltype(T::address) val;
    eq_addr(decltype(T::address) val) : val(val) {}
    bool operator()(const T &test)
    {
        return test.address == val;
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
        uint32_t set = get_set(fill_mshr->address), way;
        if (cache_type == IS_LLC)
            way = llc_find_victim(fill_cpu, fill_mshr->instr_id, set, block[set], fill_mshr->ip, fill_mshr->full_addr, fill_mshr->type);
        else
            way = find_victim(fill_cpu, fill_mshr->instr_id, set, block[set], fill_mshr->ip, fill_mshr->full_addr, fill_mshr->type);

        bool bypass = (way == NUM_WAY);
#ifndef LLC_BYPASS
        assert(!bypass);
#endif

        bool evicting_dirty = (lower_level != NULL) && block[set][way].dirty;

        // In the case where we would evict a dirty block, give up and stall if the lower-level WQ is full.
        if (!bypass && evicting_dirty && (lower_level->get_occupancy(2, block[set][way].address) == lower_level->get_size(2, block[set][way].address)))
        {
            // lower level WQ is full, cannot replace this victim
            lower_level->increment_WQ_FULL(block[set][way].address);
            STALL[fill_mshr->type]++;

            DP ( if (warmup_complete[fill_cpu]) {
                    std::cout << "[" << NAME << "] " << __func__ << " stopping fill because";
                    std::cout << " lower level wq is full!" << " fill_addr: " << std::hex << fill_mshr->address;
                    std::cout << " victim_addr: " << block[set][way].tag << std::dec << std::endl; });
            return;
        }

        if (!bypass)
        {
            if (evicting_dirty)
            {
                PACKET writeback_packet;

                writeback_packet.fill_level = fill_level << 1;
                writeback_packet.cpu = fill_cpu;
                writeback_packet.address = block[set][way].address;
                writeback_packet.full_addr = block[set][way].full_addr;
                writeback_packet.data = block[set][way].data;
                writeback_packet.instr_id = fill_mshr->instr_id;
                writeback_packet.ip = 0; // writeback does not have ip
                writeback_packet.type = WRITEBACK;
                writeback_packet.event_cycle = current_core_cycle[fill_cpu];

                lower_level->add_wq(&writeback_packet);
            }

            fill_cache(set, way, &(*fill_mshr));

            // RFO marks cache line dirty
            if (fill_mshr->type == RFO && cache_type == IS_L1D)
                block[set][way].dirty = 1;
        }

        if(warmup_complete[fill_cpu] && (fill_mshr->cycle_enqueued != 0))
            total_miss_latency += current_core_cycle[fill_cpu] - fill_mshr->cycle_enqueued;

        // update prefetcher
        if (cache_type == IS_L1I)
            l1i_prefetcher_cache_fill(fill_cpu, ((fill_mshr->ip)>>LOG2_BLOCK_SIZE)<<LOG2_BLOCK_SIZE, set, way, fill_mshr->type == PREFETCH, ((block[set][way].ip)>>LOG2_BLOCK_SIZE)<<LOG2_BLOCK_SIZE);
        if (cache_type == IS_L1D)
            l1d_prefetcher_cache_fill(fill_mshr->full_v_addr, fill_mshr->full_addr, set, way, fill_mshr->type == PREFETCH, block[set][way].address<<LOG2_BLOCK_SIZE, fill_mshr->pf_metadata);
        if  (cache_type == IS_L2C)
            fill_mshr->pf_metadata = l2c_prefetcher_cache_fill((fill_mshr->v_address)<<LOG2_BLOCK_SIZE, (fill_mshr->address)<<LOG2_BLOCK_SIZE, set, way, fill_mshr->type == PREFETCH, (block[set][way].address)<<LOG2_BLOCK_SIZE, fill_mshr->pf_metadata);
        if (cache_type == IS_LLC)
        {
            cpu = fill_cpu;
            fill_mshr->pf_metadata = llc_prefetcher_cache_fill((fill_mshr->v_address)<<LOG2_BLOCK_SIZE, (fill_mshr->address)<<LOG2_BLOCK_SIZE, set, way, fill_mshr->type == PREFETCH, (block[set][way].address)<<LOG2_BLOCK_SIZE, fill_mshr->pf_metadata);
            cpu = 0;
        }


        // update replacement policy
        if (cache_type == IS_LLC)
            llc_update_replacement_state(fill_cpu, set, way, fill_mshr->full_addr, fill_mshr->ip, 0, fill_mshr->type, 0);
        else
            update_replacement_state(fill_cpu, set, way, fill_mshr->full_addr, fill_mshr->ip, 0, fill_mshr->type, 0);

        // check fill level
        if (fill_mshr->fill_level < fill_level) {
            if (fill_mshr->instruction)
                upper_level_icache[fill_cpu]->return_data(&(*fill_mshr));
            if (fill_mshr->is_data)
                upper_level_dcache[fill_cpu]->return_data(&(*fill_mshr));
        }

        // update processed packets
        if (cache_type == IS_ITLB) {
            fill_mshr->instruction_pa = block[set][way].data;
            if (PROCESSED.occupancy < PROCESSED.SIZE)
                PROCESSED.add_queue(&(*fill_mshr));
        }
        else if (cache_type == IS_DTLB) {
            fill_mshr->data_pa = block[set][way].data;
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
    bool continue_write = true;
    while (continue_write && writes_available_this_cycle > 0)
    {
        // handle the oldest entry
        if ((WQ.occupancy == 0) || (WQ.entry[WQ.head].cpu >= NUM_CPUS) || (WQ.entry[WQ.head].event_cycle > current_core_cycle[WQ.entry[WQ.head].cpu]))
            return;

        // access cache
        uint32_t writeback_cpu = WQ.entry[WQ.head].cpu;
        uint32_t set = get_set(WQ.entry[WQ.head].address);
        uint32_t way = get_way(WQ.entry[WQ.head].address, set);

        if (way < NUM_WAY) // HIT
        {
            if (cache_type == IS_LLC)
                llc_update_replacement_state(writeback_cpu, set, way, block[set][way].full_addr, WQ.entry[WQ.head].ip, 0, WQ.entry[WQ.head].type, 1);
            else
                update_replacement_state(writeback_cpu, set, way, block[set][way].full_addr, WQ.entry[WQ.head].ip, 0, WQ.entry[WQ.head].type, 1);

            // COLLECT STATS
            sim_hit[writeback_cpu][WQ.entry[WQ.head].type]++;
            sim_access[writeback_cpu][WQ.entry[WQ.head].type]++;

            // mark dirty
            block[set][way].dirty = 1;

            if (cache_type == IS_ITLB)
                WQ.entry[WQ.head].instruction_pa = block[set][way].data;
            else if (cache_type == IS_DTLB)
                WQ.entry[WQ.head].data_pa = block[set][way].data;
            else if (cache_type == IS_STLB)
                WQ.entry[WQ.head].data = block[set][way].data;

            // check fill level
            if (WQ.entry[WQ.head].fill_level < fill_level)
            {
                if (WQ.entry[WQ.head].instruction)
                    upper_level_icache[writeback_cpu]->return_data(&WQ.entry[WQ.head]);
                if (WQ.entry[WQ.head].is_data)
                    upper_level_dcache[writeback_cpu]->return_data(&WQ.entry[WQ.head]);
            }

            HIT[WQ.entry[WQ.head].type]++;
            ACCESS[WQ.entry[WQ.head].type]++;

            writes_available_this_cycle--;

            // remove this entry from WQ
            WQ.remove_queue(&WQ.entry[WQ.head]);
        }
        else // MISS
        {
            DP ( if (warmup_complete[writeback_cpu]) {
                    cout << "[" << NAME << "] " << __func__ << " type: " << +WQ.entry[WQ.head].type << " miss";
                    cout << " instr_id: " << WQ.entry[WQ.head].instr_id << " address: " << hex << WQ.entry[WQ.head].address;
                    cout << " full_addr: " << WQ.entry[WQ.head].full_addr << dec;
                    cout << " cycle: " << WQ.entry[WQ.head].event_cycle << endl; });

            if (cache_type == IS_L1D) { // RFO miss

                // check mshr
                auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(WQ.entry[WQ.head].address));
                bool mshr_full = std::none_of(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(0));

                if ((mshr_entry == MSHR.end()) && !mshr_full) // this is a new miss
                {
                    if(cache_type == IS_LLC)
                    {
                        // check to make sure the DRAM RQ has room for this LLC RFO miss
                        if (lower_level->get_occupancy(1, WQ.entry[WQ.head].address) == lower_level->get_size(1, WQ.entry[WQ.head].address))
                        {
                            continue_write = false;
                        }
                        else
                        {
                            add_mshr(&WQ.entry[WQ.head]);
                            lower_level->add_rq(&WQ.entry[WQ.head]);
                        }
                    }
                    else
                    {
                        // add it to mshr (RFO miss)
                        add_mshr(&WQ.entry[WQ.head]);

                        // add it to the next level's read queue
                        lower_level->add_rq(&WQ.entry[WQ.head]);
                    }
                }
                else if ((mshr_entry == MSHR.end()) && mshr_full) // not enough MSHR resource
                {
                    // cannot handle miss request until one of MSHRs is available
                    continue_write = false;
                    STALL[WQ.entry[WQ.head].type]++;
                }
                else // already in-flight miss
                {
                    // update fill_level
                    if (WQ.entry[WQ.head].fill_level < mshr_entry->fill_level)
                        mshr_entry->fill_level = WQ.entry[WQ.head].fill_level;

                    if((WQ.entry[WQ.head].fill_l1i) && (mshr_entry->fill_l1i != 1))
                    {
                        mshr_entry->fill_l1i = 1;
                    }
                    if((WQ.entry[WQ.head].fill_l1d) && (mshr_entry->fill_l1d != 1))
                    {
                        mshr_entry->fill_l1d = 1;
                    }

                    // update request
                    if (mshr_entry->type == PREFETCH) {
                        uint8_t  prior_returned = mshr_entry->returned;
                        uint64_t prior_event_cycle = mshr_entry->event_cycle;
                        *mshr_entry = WQ.entry[WQ.head];

                        // in case request is already returned, we should keep event_cycle and retunred variables
                        mshr_entry->returned = prior_returned;
                        mshr_entry->event_cycle = prior_event_cycle;
                    }

                    MSHR_MERGED[WQ.entry[WQ.head].type]++;

                    DP ( if (warmup_complete[writeback_cpu]) {
                            cout << "[" << NAME << "] " << __func__ << " mshr merged";
                            cout << " instr_id: " << WQ.entry[WQ.head].instr_id << " prior_id: " << MSHR[mshr_entry].instr_id; 
                            cout << " address: " << hex << WQ.entry[WQ.head].address;
                            cout << " full_addr: " << WQ.entry[WQ.head].full_addr << dec;
                            cout << " cycle: " << WQ.entry[WQ.head].event_cycle << endl; });
                }
            }
            else {
                // find victim
                uint32_t set = get_set(WQ.entry[WQ.head].address), way;
                if (cache_type == IS_LLC)
                    way = llc_find_victim(writeback_cpu, WQ.entry[WQ.head].instr_id, set, block[set], WQ.entry[WQ.head].ip, WQ.entry[WQ.head].full_addr, WQ.entry[WQ.head].type);
                else
                    way = find_victim(writeback_cpu, WQ.entry[WQ.head].instr_id, set, block[set], WQ.entry[WQ.head].ip, WQ.entry[WQ.head].full_addr, WQ.entry[WQ.head].type);

                assert(way < LLC_WAY);

                // is this dirty?
                if (block[set][way].dirty && lower_level != NULL)
                {
                    if (lower_level->get_occupancy(2, block[set][way].address) == lower_level->get_size(2, block[set][way].address)) {

                        // lower level WQ is full, cannot replace this victim
                        continue_write = false;
                        lower_level->increment_WQ_FULL(block[set][way].address);
                        STALL[WQ.entry[WQ.head].type]++;

                        DP ( if (warmup_complete[writeback_cpu]) {
                                std::cout << "[" << NAME << "] " << __func__ << " ceasing write. ";
                                std::cout << " Lower level wq is full!" << " fill_addr: " << std::hex << WQ.entry[WQ.head].address;
                                std::cout << " victim_addr: " << block[set][way].tag << std::dec << std::endl; });
                    }
                    else { 
                        PACKET writeback_packet;

                        writeback_packet.fill_level = fill_level << 1;
                        writeback_packet.cpu = writeback_cpu;
                        writeback_packet.address = block[set][way].address;
                        writeback_packet.full_addr = block[set][way].full_addr;
                        writeback_packet.data = block[set][way].data;
                        writeback_packet.instr_id = WQ.entry[WQ.head].instr_id;
                        writeback_packet.ip = 0;
                        writeback_packet.type = WRITEBACK;
                        writeback_packet.event_cycle = current_core_cycle[writeback_cpu];

                        lower_level->add_wq(&writeback_packet);
                    }
                }

                if (continue_write) {
                    // update prefetcher
                    if (cache_type == IS_L1I)
                        l1i_prefetcher_cache_fill(writeback_cpu, ((WQ.entry[WQ.head].ip)>>LOG2_BLOCK_SIZE)<<LOG2_BLOCK_SIZE, set, way, 0, ((block[set][way].ip)>>LOG2_BLOCK_SIZE)<<LOG2_BLOCK_SIZE);
                    if (cache_type == IS_L1D)
                        l1d_prefetcher_cache_fill(WQ.entry[WQ.head].full_v_addr, WQ.entry[WQ.head].full_addr, set, way, 0, block[set][way].address<<LOG2_BLOCK_SIZE, WQ.entry[WQ.head].pf_metadata);
                    else if (cache_type == IS_L2C)
                        WQ.entry[WQ.head].pf_metadata = l2c_prefetcher_cache_fill((WQ.entry[WQ.head].v_address)<<LOG2_BLOCK_SIZE, (WQ.entry[WQ.head].address)<<LOG2_BLOCK_SIZE, set, way, 0,
                                (block[set][way].address)<<LOG2_BLOCK_SIZE, WQ.entry[WQ.head].pf_metadata);
                    if (cache_type == IS_LLC)
                    {
                        cpu = writeback_cpu;
                        WQ.entry[WQ.head].pf_metadata =llc_prefetcher_cache_fill((WQ.entry[WQ.head].v_address)<<LOG2_BLOCK_SIZE, (WQ.entry[WQ.head].address)<<LOG2_BLOCK_SIZE, set, way, 0,
                                (block[set][way].address)<<LOG2_BLOCK_SIZE, WQ.entry[WQ.head].pf_metadata);
                        cpu = 0;
                    }

                    // update replacement policy
                    if (cache_type == IS_LLC)
                        llc_update_replacement_state(writeback_cpu, set, way, WQ.entry[WQ.head].full_addr, WQ.entry[WQ.head].ip, block[set][way].full_addr, WQ.entry[WQ.head].type, 0);
                    else
                        update_replacement_state(writeback_cpu, set, way, WQ.entry[WQ.head].full_addr, WQ.entry[WQ.head].ip, block[set][way].full_addr, WQ.entry[WQ.head].type, 0);

                    // COLLECT STATS
                    sim_miss[writeback_cpu][WQ.entry[WQ.head].type]++;
                    sim_access[writeback_cpu][WQ.entry[WQ.head].type]++;

                    fill_cache(set, way, &WQ.entry[WQ.head]);

                    // mark dirty
                    block[set][way].dirty = 1; 

                    // check fill level
                    if (WQ.entry[WQ.head].fill_level < fill_level) {

                        if (WQ.entry[WQ.head].instruction)
                            upper_level_icache[writeback_cpu]->return_data(&WQ.entry[WQ.head]);
                        if (WQ.entry[WQ.head].is_data)
                            upper_level_dcache[writeback_cpu]->return_data(&WQ.entry[WQ.head]);
                    }
                }
            }

            if (continue_write)
            {
                MISS[WQ.entry[WQ.head].type]++;
                ACCESS[WQ.entry[WQ.head].type]++;

                writes_available_this_cycle--;

                // remove this entry from WQ
                WQ.remove_queue(&WQ.entry[WQ.head]);
            }
        }
    }
}

void CACHE::handle_read()
{
    bool continue_read = true;
    while (continue_read && reads_available_this_cycle > 0) {

        // handle the oldest entry
        if ((RQ.occupancy == 0) || (RQ.entry[RQ.head].cpu >= NUM_CPUS) || (RQ.entry[RQ.head].event_cycle > current_core_cycle[RQ.entry[RQ.head].cpu]))
            return;

        uint32_t read_cpu = RQ.entry[RQ.head].cpu;
        uint32_t set = get_set(RQ.entry[RQ.head].address);
        uint32_t way = get_way(RQ.entry[RQ.head].address, set);

        if (way < NUM_WAY) // HIT
        {
            if (cache_type == IS_ITLB) {
                RQ.entry[RQ.head].instruction_pa = block[set][way].data;
                if (PROCESSED.occupancy < PROCESSED.SIZE)
                    PROCESSED.add_queue(&RQ.entry[RQ.head]);
            }
            else if (cache_type == IS_DTLB) {
                RQ.entry[RQ.head].data_pa = block[set][way].data;
                if (PROCESSED.occupancy < PROCESSED.SIZE)
                    PROCESSED.add_queue(&RQ.entry[RQ.head]);
            }
            else if (cache_type == IS_STLB) 
                RQ.entry[RQ.head].data = block[set][way].data;
            else if (cache_type == IS_L1I) {
                if (PROCESSED.occupancy < PROCESSED.SIZE)
                    PROCESSED.add_queue(&RQ.entry[RQ.head]);
            }
            else if ((cache_type == IS_L1D) && (RQ.entry[RQ.head].type != PREFETCH)) {
                if (PROCESSED.occupancy < PROCESSED.SIZE)
                    PROCESSED.add_queue(&RQ.entry[RQ.head]);
            }

            // update prefetcher on load instruction
            if (RQ.entry[RQ.head].type == LOAD) {
                if(cache_type == IS_L1I)
                    l1i_prefetcher_cache_operate(read_cpu, RQ.entry[RQ.head].ip, 1, block[set][way].prefetch);
                if (cache_type == IS_L1D) 
                    l1d_prefetcher_operate(RQ.entry[RQ.head].full_v_addr, RQ.entry[RQ.head].full_addr, RQ.entry[RQ.head].ip, 1, RQ.entry[RQ.head].type);
                else if (cache_type == IS_L2C)
                    l2c_prefetcher_operate((RQ.entry[RQ.head].v_address)<<LOG2_BLOCK_SIZE, (block[set][way].address)<<LOG2_BLOCK_SIZE, RQ.entry[RQ.head].ip, 1, RQ.entry[RQ.head].type, 0);
                else if (cache_type == IS_LLC)
                {
                    cpu = read_cpu;
                    llc_prefetcher_operate((RQ.entry[RQ.head].v_address)<<LOG2_BLOCK_SIZE, (block[set][way].address)<<LOG2_BLOCK_SIZE, RQ.entry[RQ.head].ip, 1, RQ.entry[RQ.head].type, 0);
                    cpu = 0;
                }
            }

            // update replacement policy
            if (cache_type == IS_LLC)
                llc_update_replacement_state(read_cpu, set, way, block[set][way].full_addr, RQ.entry[RQ.head].ip, 0, RQ.entry[RQ.head].type, 1);
            else
                update_replacement_state(read_cpu, set, way, block[set][way].full_addr, RQ.entry[RQ.head].ip, 0, RQ.entry[RQ.head].type, 1);

            // COLLECT STATS
            sim_hit[read_cpu][RQ.entry[RQ.head].type]++;
            sim_access[read_cpu][RQ.entry[RQ.head].type]++;

            // check fill level
            if (RQ.entry[RQ.head].fill_level < fill_level)
            {
                if (RQ.entry[RQ.head].instruction)
                    upper_level_icache[read_cpu]->return_data(&RQ.entry[RQ.head]);
                if (RQ.entry[RQ.head].is_data)
                    upper_level_dcache[read_cpu]->return_data(&RQ.entry[RQ.head]);
            }

            // update prefetch stats and reset prefetch bit
            if (block[set][way].prefetch) {
                pf_useful++;
                block[set][way].prefetch = 0;
            }
            block[set][way].used = 1;

            HIT[RQ.entry[RQ.head].type]++;
            ACCESS[RQ.entry[RQ.head].type]++;

            // remove this entry from RQ
            RQ.remove_queue(&RQ.entry[RQ.head]);
            reads_available_this_cycle--;
        }
        else { // read miss

            DP ( if (warmup_complete[read_cpu]) {
                    cout << "[" << NAME << "] " << __func__ << " read miss";
                    cout << " instr_id: " << RQ.entry[RQ.head].instr_id << " address: " << hex << RQ.entry[RQ.head].address;
                    cout << " full_addr: " << RQ.entry[RQ.head].full_addr << dec;
                    cout << " cycle: " << RQ.entry[RQ.head].event_cycle << endl; });

            // check mshr
            auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(RQ.entry[RQ.head].address));
            bool mshr_full = std::none_of(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(0));

            if ((mshr_entry == MSHR.end()) && !mshr_full) { // this is a new miss

                if(cache_type == IS_LLC)
                {
                    // check to make sure the DRAM RQ has room for this LLC read miss
                    if (lower_level->get_occupancy(1, RQ.entry[RQ.head].address) == lower_level->get_size(1, RQ.entry[RQ.head].address))
                    {
                        continue_read = false;
                    }
                    else
                    {
                        add_mshr(&RQ.entry[RQ.head]);
                        if(lower_level)
                        {
                            lower_level->add_rq(&RQ.entry[RQ.head]);
                        }
                    }
                }
                else
                {
                    // add it to mshr (read miss)
                    add_mshr(&RQ.entry[RQ.head]);

                    // add it to the next level's read queue
                    if (lower_level)
                        lower_level->add_rq(&RQ.entry[RQ.head]);
                    else { // this is the last level
                        if (cache_type == IS_STLB) {
                            // TODO: need to differentiate page table walk and actual swap

                            // emulate page table walk
                            uint64_t pa = vmem.va_to_pa(read_cpu, RQ.entry[RQ.head].full_addr);

                            RQ.entry[RQ.head].data = pa >> LOG2_PAGE_SIZE; 
                            RQ.entry[RQ.head].event_cycle = current_core_cycle[read_cpu];
                            return_data(&RQ.entry[RQ.head]);
                        }
                    }
                }
            }
            else if ((mshr_entry == MSHR.end()) && mshr_full) { // not enough MSHR resource

                // cannot handle miss request until one of MSHRs is available
                continue_read = false;
                STALL[RQ.entry[RQ.head].type]++;
            }
            else
            {

                // mark merged consumer
                if (RQ.entry[RQ.head].type == RFO) {

                    if (RQ.entry[RQ.head].tlb_access) {
                        mshr_entry->sq_index_depend_on_me.insert (RQ.entry[RQ.head].sq_index);
                        mshr_entry->sq_index_depend_on_me.join (RQ.entry[RQ.head].sq_index_depend_on_me, SQ_SIZE);
                    }

                    mshr_entry->lq_index_depend_on_me.join (RQ.entry[RQ.head].lq_index_depend_on_me, LQ_SIZE);
                }
                else {
                    if (RQ.entry[RQ.head].instruction) {
                        mshr_entry->instruction = 1; // add as instruction type

                        DP (if (warmup_complete[mshr_entry->cpu]) {
                                cout << "[INSTR_MERGED] " << __func__ << " cpu: " << mshr_entry->cpu << " instr_id: " << mshr_entry->instr_id;
                                cout << " merged rob_index: " << RQ.entry[RQ.head].rob_index << " instr_id: " << RQ.entry[RQ.head].instr_id << endl; });
                    }
                    else
                    {
                        uint32_t lq_index = RQ.entry[RQ.head].lq_index;
                        mshr_entry->is_data = 1; // add as data type
                        mshr_entry->lq_index_depend_on_me.insert (lq_index);

                        DP (if (warmup_complete[read_cpu]) {
                                cout << "[DATA_MERGED] " << __func__ << " cpu: " << read_cpu << " instr_id: " << RQ.entry[RQ.head].instr_id;
                                cout << " merged rob_index: " << RQ.entry[RQ.head].rob_index << " instr_id: " << RQ.entry[RQ.head].instr_id << " lq_index: " << RQ.entry[RQ.head].lq_index << endl; });
                        mshr_entry->lq_index_depend_on_me.join (RQ.entry[RQ.head].lq_index_depend_on_me, LQ_SIZE);
                        mshr_entry->sq_index_depend_on_me.join (RQ.entry[RQ.head].sq_index_depend_on_me, SQ_SIZE);
                    }
                }

                // update fill_level
                if (RQ.entry[RQ.head].fill_level < mshr_entry->fill_level)
                    mshr_entry->fill_level = RQ.entry[RQ.head].fill_level;

                if((RQ.entry[RQ.head].fill_l1i) && (mshr_entry->fill_l1i != 1))
                {
                    mshr_entry->fill_l1i = 1;
                }
                if((RQ.entry[RQ.head].fill_l1d) && (mshr_entry->fill_l1d != 1))
                {
                    mshr_entry->fill_l1d = 1;
                }

                // update request
                if (mshr_entry->type == PREFETCH) {
                    uint8_t  prior_returned = mshr_entry->returned;
                    uint64_t prior_event_cycle = mshr_entry->event_cycle;
                    uint8_t  prior_fill_l1i = mshr_entry->fill_l1i;
                    uint8_t  prior_fill_l1d = mshr_entry->fill_l1d;

                    *mshr_entry = RQ.entry[RQ.head];

                    if(prior_fill_l1i && mshr_entry->fill_l1i == 0)
                        mshr_entry->fill_l1i = 1;
                    if(prior_fill_l1d && mshr_entry->fill_l1d == 0)
                        mshr_entry->fill_l1d = 1;

                    // in case request is already returned, we should keep event_cycle and retunred variables
                    mshr_entry->returned = prior_returned;
                    mshr_entry->event_cycle = prior_event_cycle;
                }

                MSHR_MERGED[RQ.entry[RQ.head].type]++;

                DP ( if (warmup_complete[read_cpu]) {
                        cout << "[" << NAME << "] " << __func__ << " mshr merged";
                        cout << " instr_id: " << RQ.entry[RQ.head].instr_id << " prior_id: " << MSHR[mshr_entry].instr_id; 
                        cout << " address: " << hex << RQ.entry[RQ.head].address;
                        cout << " full_addr: " << RQ.entry[RQ.head].full_addr << dec;
                        cout << " cycle: " << RQ.entry[RQ.head].event_cycle << endl; });
            }

            if (continue_read) {
                // update prefetcher on load instruction
                if (RQ.entry[RQ.head].type == LOAD) {
                    if(cache_type == IS_L1I)
                        l1i_prefetcher_cache_operate(read_cpu, RQ.entry[RQ.head].ip, 0, 0);
                    if (cache_type == IS_L1D) 
                        l1d_prefetcher_operate(RQ.entry[RQ.head].full_v_addr, RQ.entry[RQ.head].full_addr, RQ.entry[RQ.head].ip, 0, RQ.entry[RQ.head].type);
                    if (cache_type == IS_L2C)
                        l2c_prefetcher_operate((RQ.entry[RQ.head].v_address)<<LOG2_BLOCK_SIZE, (RQ.entry[RQ.head].address)<<LOG2_BLOCK_SIZE, RQ.entry[RQ.head].ip, 0, RQ.entry[RQ.head].type, 0);
                    if (cache_type == IS_LLC)
                    {
                        cpu = read_cpu;
                        llc_prefetcher_operate((RQ.entry[RQ.head].v_address)<<LOG2_BLOCK_SIZE, (RQ.entry[RQ.head].address)<<LOG2_BLOCK_SIZE, RQ.entry[RQ.head].ip, 0, RQ.entry[RQ.head].type, 0);
                        cpu = 0;
                    }
                }

                MISS[RQ.entry[RQ.head].type]++;
                ACCESS[RQ.entry[RQ.head].type]++;

                // remove this entry from RQ
                RQ.remove_queue(&RQ.entry[RQ.head]);
                reads_available_this_cycle--;
            }
        }
    }
}

void CACHE::handle_prefetch()
{
    bool continue_prefetch = true;
    while (continue_prefetch && reads_available_this_cycle > 0)
    {
        // handle the oldest entry
        if ((PQ.occupancy == 0) || (PQ.entry[PQ.head].cpu >= NUM_CPUS) || (PQ.entry[PQ.head].event_cycle > current_core_cycle[PQ.entry[PQ.head].cpu]))
            return;

        uint32_t prefetch_cpu = PQ.entry[PQ.head].cpu;
        uint32_t set = get_set(PQ.entry[PQ.head].address);
        uint32_t way = get_way(PQ.entry[PQ.head].address, set);

        if (way < NUM_WAY) // HIT
        {
            // update replacement policy
            if (cache_type == IS_LLC)
                llc_update_replacement_state(prefetch_cpu, set, way, block[set][way].full_addr, PQ.entry[PQ.head].ip, 0, PQ.entry[PQ.head].type, 1);
            else
                update_replacement_state(prefetch_cpu, set, way, block[set][way].full_addr, PQ.entry[PQ.head].ip, 0, PQ.entry[PQ.head].type, 1);

            // COLLECT STATS
            sim_hit[prefetch_cpu][PQ.entry[PQ.head].type]++;
            sim_access[prefetch_cpu][PQ.entry[PQ.head].type]++;

            // run prefetcher on prefetches from higher caches
            if(PQ.entry[PQ.head].pf_origin_level < fill_level)
            {
                if (cache_type == IS_L1D)
                    l1d_prefetcher_operate(PQ.entry[PQ.head].full_v_addr, PQ.entry[PQ.head].full_addr, PQ.entry[PQ.head].ip, 1, PREFETCH);
                else if (cache_type == IS_L2C)
                    PQ.entry[PQ.head].pf_metadata = l2c_prefetcher_operate((PQ.entry[PQ.head].v_address)<<LOG2_BLOCK_SIZE, (block[set][way].address)<<LOG2_BLOCK_SIZE, PQ.entry[PQ.head].ip, 1, PREFETCH, PQ.entry[PQ.head].pf_metadata);
                else if (cache_type == IS_LLC)
                {
                    cpu = prefetch_cpu;
                    PQ.entry[PQ.head].pf_metadata = llc_prefetcher_operate((PQ.entry[PQ.head].v_address)<<LOG2_BLOCK_SIZE, (block[set][way].address)<<LOG2_BLOCK_SIZE, PQ.entry[PQ.head].ip, 1, PREFETCH, PQ.entry[PQ.head].pf_metadata);
                    cpu = 0;
                }
            }

            // check fill level
            if (PQ.entry[PQ.head].fill_level < fill_level) {
                if (PQ.entry[PQ.head].instruction)
                    upper_level_icache[prefetch_cpu]->return_data(&PQ.entry[PQ.head]);
                if (PQ.entry[PQ.head].is_data)
                    upper_level_dcache[prefetch_cpu]->return_data(&PQ.entry[PQ.head]);
            }

            HIT[PQ.entry[PQ.head].type]++;
            ACCESS[PQ.entry[PQ.head].type]++;

            // remove this entry from PQ
            PQ.remove_queue(&PQ.entry[PQ.head]);
            reads_available_this_cycle--;
        }
        else { // prefetch miss

            DP ( if (warmup_complete[prefetch_cpu]) {
                    cout << "[" << NAME << "] " << __func__ << " prefetch miss";
                    cout << " instr_id: " << PQ.entry[PQ.head].instr_id << " address: " << hex << PQ.entry[PQ.head].address;
                    cout << " full_addr: " << PQ.entry[PQ.head].full_addr << dec << " fill_level: " << PQ.entry[PQ.head].fill_level;
                    cout << " cycle: " << PQ.entry[PQ.head].event_cycle << endl; });

            // check mshr
            auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(PQ.entry[PQ.head].address));
            bool mshr_full = std::none_of(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(0));

            if ((mshr_entry == MSHR.end()) && !mshr_full) { // this is a new miss

                DP ( if (warmup_complete[PQ.entry[PQ.head].cpu]) {
                        cout << "[" << NAME << "_PQ] " <<  __func__ << " want to add instr_id: " << PQ.entry[PQ.head].instr_id << " address: " << hex << PQ.entry[PQ.head].address;
                        cout << " full_addr: " << PQ.entry[PQ.head].full_addr << dec;
                        cout << " occupancy: " << lower_level->get_occupancy(3, PQ.entry[PQ.head].address) << " SIZE: " << lower_level->get_size(3, PQ.entry[PQ.head].address) << endl; });

                // first check if the lower level PQ is full or not
                // this is possible since multiple prefetchers can exist at each level of caches
                if (lower_level) {
                    if (cache_type == IS_LLC) {
                        if (lower_level->get_occupancy(1, PQ.entry[PQ.head].address) == lower_level->get_size(1, PQ.entry[PQ.head].address))
                            continue_prefetch = false;
                        else {

                            // run prefetcher on prefetches from higher caches
                            if(PQ.entry[PQ.head].pf_origin_level < fill_level)
                            {
                                if (cache_type == IS_LLC)
                                {
                                    cpu = prefetch_cpu;
                                    PQ.entry[PQ.head].pf_metadata = llc_prefetcher_operate((PQ.entry[PQ.head].v_address)<<LOG2_BLOCK_SIZE, (PQ.entry[PQ.head].address)<<LOG2_BLOCK_SIZE,
                                            PQ.entry[PQ.head].ip, 0, PREFETCH, PQ.entry[PQ.head].pf_metadata);
                                    cpu = 0;
                                }
                            }

                            // add it to MSHRs if this prefetch miss will be filled to this cache level
                            if (PQ.entry[PQ.head].fill_level <= fill_level)
                                add_mshr(&PQ.entry[PQ.head]);

                            lower_level->add_rq(&PQ.entry[PQ.head]); // add it to the DRAM RQ
                        }
                    }
                    else {
                        if (lower_level->get_occupancy(3, PQ.entry[PQ.head].address) == lower_level->get_size(3, PQ.entry[PQ.head].address))
                            continue_prefetch = false;
                        else {

                            // run prefetcher on prefetches from higher caches
                            if(PQ.entry[PQ.head].pf_origin_level < fill_level)
                            {
                                if (cache_type == IS_L1D)
                                    l1d_prefetcher_operate(PQ.entry[PQ.head].full_v_addr, PQ.entry[PQ.head].full_addr, PQ.entry[PQ.head].ip, 0, PREFETCH);
                                if (cache_type == IS_L2C)
                                    PQ.entry[PQ.head].pf_metadata = l2c_prefetcher_operate((PQ.entry[PQ.head].v_address)<<LOG2_BLOCK_SIZE, (PQ.entry[PQ.head].address)<<LOG2_BLOCK_SIZE, PQ.entry[PQ.head].ip, 0, PREFETCH, PQ.entry[PQ.head].pf_metadata);
                            }

                            // add it to MSHRs if this prefetch miss will be filled to this cache level
                            if (PQ.entry[PQ.head].fill_level <= fill_level)
                                add_mshr(&PQ.entry[PQ.head]);

                            lower_level->add_pq(&PQ.entry[PQ.head]); // add it to the DRAM RQ
                        }
                    }
                }
            }
            else if ((mshr_entry == MSHR.end()) && mshr_full) { // not enough MSHR resource

                // TODO: should we allow prefetching with lower fill level at this case?

                // cannot handle miss request until one of MSHRs is available
                continue_prefetch = false;
                STALL[PQ.entry[PQ.head].type]++;
            }
            else // already in-flight miss
            {
                // no need to update request except fill_level
                if (PQ.entry[PQ.head].fill_level < mshr_entry->fill_level)
                    mshr_entry->fill_level = PQ.entry[PQ.head].fill_level;

                if((PQ.entry[PQ.head].fill_l1i) && (mshr_entry->fill_l1i != 1))
                {
                    mshr_entry->fill_l1i = 1;
                }
                if((PQ.entry[PQ.head].fill_l1d) && (mshr_entry->fill_l1d != 1))
                {
                    mshr_entry->fill_l1d = 1;
                }

                MSHR_MERGED[PQ.entry[PQ.head].type]++;

                DP ( if (warmup_complete[prefetch_cpu]) {
                        cout << "[" << NAME << "] " << __func__ << " mshr merged";
                        cout << " instr_id: " << PQ.entry[PQ.head].instr_id << " prior_id: " << mshr_entry->instr_id; 
                        cout << " address: " << hex << PQ.entry[PQ.head].address;
                        cout << " full_addr: " << PQ.entry[PQ.head].full_addr << dec << " fill_level: " << mshr_entry->fill_level;
                        cout << " cycle: " << mshr_entry->event_cycle << endl; });
            }

            if (continue_prefetch)
            {
                DP ( if (warmup_complete[prefetch_cpu]) {
                        cout << "[" << NAME << "] " << __func__ << " prefetch miss handled";
                        cout << " instr_id: " << PQ.entry[PQ.head].instr_id << " address: " << hex << PQ.entry[PQ.head].address;
                        cout << " full_addr: " << PQ.entry[PQ.head].full_addr << dec << " fill_level: " << PQ.entry[PQ.head].fill_level;
                        cout << " cycle: " << PQ.entry[PQ.head].event_cycle << endl; });

                MISS[PQ.entry[PQ.head].type]++;
                ACCESS[PQ.entry[PQ.head].type]++;

                // remove this entry from PQ
                PQ.remove_queue(&PQ.entry[PQ.head]);
                reads_available_this_cycle--;
            }
        }
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
    for (uint32_t way=0; way<NUM_WAY; way++) {
        if (block[set][way].valid && (block[set][way].tag == address)) 
            return way;
    }

    return NUM_WAY;
}

void CACHE::fill_cache(uint32_t set, uint32_t way, PACKET *packet)
{
    assert(cache_type != IS_ITLB || packet->data != 0);
    assert(cache_type != IS_DTLB || packet->data != 0);
    assert(cache_type != IS_STLB || packet->data != 0);

    if (block[set][way].prefetch && (block[set][way].used == 0))
        pf_useless++;

    block[set][way].valid = 1;
    block[set][way].dirty = 0;
    block[set][way].prefetch = (packet->type == PREFETCH) ? 1 : 0;
    block[set][way].used = 0;

    if (block[set][way].prefetch)
        pf_fill++;

    block[set][way].delta = packet->delta;
    block[set][way].depth = packet->depth;
    block[set][way].signature = packet->signature;
    block[set][way].confidence = packet->confidence;

    block[set][way].tag = packet->address;
    block[set][way].address = packet->address;
    block[set][way].full_addr = packet->full_addr;
    block[set][way].v_address = packet->v_address;
    block[set][way].full_v_addr = packet->full_v_addr;
    block[set][way].data = packet->data;
    block[set][way].ip = packet->ip;
    block[set][way].cpu = packet->cpu;
    block[set][way].instr_id = packet->instr_id;

    DP ( if (warmup_complete[packet->cpu]) {
    cout << "[" << NAME << "] " << __func__ << " set: " << set << " way: " << way;
    cout << " lru: " << block[set][way].lru << " tag: " << hex << block[set][way].tag << " full_addr: " << block[set][way].full_addr;
    cout << " data: " << block[set][way].data << dec << endl; });
}

int CACHE::invalidate_entry(uint64_t inval_addr)
{
    uint32_t set = get_set(inval_addr);
    uint32_t way = get_way(inval_addr, set);

    if (way < NUM_WAY)
        block[set][way].valid = 0;

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

		if (packet->instruction)
		  upper_level_icache[packet->cpu]->return_data(packet);
		if (packet->is_data)
		  upper_level_dcache[packet->cpu]->return_data(packet);
        }

        assert(cache_type != IS_ITLB);
        assert(cache_type != IS_DTLB);
        assert(cache_type != IS_STLB);

        // update processed packets
        if ((cache_type == IS_L1D) && (packet->type != PREFETCH)) {
            if (PROCESSED.occupancy < PROCESSED.SIZE)
                PROCESSED.add_queue(packet);

            DP ( if (warmup_complete[packet->cpu]) {
            cout << "[" << NAME << "_RQ] " << __func__ << " instr_id: " << packet->instr_id << " found recent writebacks";
            cout << hex << " read: " << packet->address << " writeback: " << WQ.entry[wq_index].address << dec;
            cout << " index: " << MAX_READ << " rob_signal: " << packet->rob_signal << endl; });
        }

        HIT[packet->type]++;
        ACCESS[packet->type]++;

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

	if((packet->fill_l1i) && (RQ.entry[index].fill_l1i != 1))
	  {
	    RQ.entry[index].fill_l1i = 1;
	  }
	if((packet->fill_l1d) && (RQ.entry[index].fill_l1d != 1))
	  {
	    RQ.entry[index].fill_l1d = 1;
	  }

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
        cerr << "[" << NAME << "_ERROR] " << __func__ << " is not empty index: " << index;
        cerr << " address: " << hex << WQ.entry[index].address;
        cerr << " full_addr: " << WQ.entry[index].full_addr << dec << endl;
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
      cout << "va_prefetch_line() pf_addr cannot be 0! exiting" << endl;
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
		if(packet->fill_l1i)
		  {
		    upper_level_icache[packet->cpu]->return_data(packet);
		  }
		if(packet->fill_l1d)
		  {
		    upper_level_dcache[packet->cpu]->return_data(packet);
		  }
	      }
	    else
	      {
		if (packet->instruction)
		  upper_level_icache[packet->cpu]->return_data(packet);
		if (packet->is_data)
		  upper_level_dcache[packet->cpu]->return_data(packet);
	      }
        }

        HIT[packet->type]++;
        ACCESS[packet->type]++;

        WQ.FORWARD++;
        PQ.ACCESS++;

        return -1;
    }

    // check for duplicates in the PQ
    int index = PQ.check_queue(packet);
    if (index != -1) {
        if (packet->fill_level < PQ.entry[index].fill_level)
	  {
            PQ.entry[index].fill_level = packet->fill_level;
	  }
	if((packet->instruction == 1) && (PQ.entry[index].instruction != 1))
	  {
	    PQ.entry[index].instruction = 1;
	  }
	if((packet->is_data == 1) && (PQ.entry[index].is_data != 1))
	  {
	    PQ.entry[index].is_data = 1;
	  }
	if((packet->fill_l1i) && (PQ.entry[index].fill_l1i != 1))
	  {
	    PQ.entry[index].fill_l1i = 1;
	  }
	if((packet->fill_l1d) && (PQ.entry[index].fill_l1d != 1))
	  {
	    PQ.entry[index].fill_l1d = 1;
	  }

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
        cerr << "[" << NAME << "_MSHR] " << __func__ << " instr_id: " << packet->instr_id << " cannot find a matching entry!";
        cerr << " full_addr: " << hex << packet->full_addr;
        cerr << " address: " << packet->address << dec;
        cerr << " event: " << packet->event_cycle << " current: " << current_core_cycle[packet->cpu] << endl;
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
    auto it = std::find_if(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(0));
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
        return std::count_if(MSHR.begin(), MSHR.end(), [](PACKET x){ return x.address != 0; });
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
