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

template <typename T>
struct eq_full_addr
{
    using argument_type = T;
    const decltype(argument_type::address) val;
    eq_full_addr(decltype(argument_type::address) val) : val(val) {}
    bool operator()(const argument_type &test)
    {
        is_valid<argument_type> validtest;
        return validtest(test) && test.full_addr == val;
    }
};

void CACHE::handle_fill()
{
    while (writes_available_this_cycle > 0)
    {
        auto fill_mshr = MSHR.begin();
        if (fill_mshr->returned != COMPLETED || fill_mshr->event_cycle > current_core_cycle[fill_mshr->cpu])
            return;

        // find victim
        uint32_t set = get_set(fill_mshr->address);
        uint32_t way = find_victim(fill_mshr->cpu, fill_mshr->instr_id, set, &block.data()[set*NUM_WAY], fill_mshr->ip, fill_mshr->full_addr, fill_mshr->type);

        bool success = filllike_miss(set, way, *fill_mshr);
        if (!success)
            return;

        if (way != NUM_WAY)
        {
            // update processed packets
            fill_mshr->data = block[set*NUM_WAY + way].data;

            for (auto ret : fill_mshr->to_return)
                ret->return_data(&(*fill_mshr));
        }

        PACKET empty;
        *fill_mshr = empty;

        writes_available_this_cycle--;
        MSHR.sort(min_fill_index());
    }
}

void CACHE::handle_writeback()
{
    while (writes_available_this_cycle > 0)
    {
        if (!WQ.has_ready() || (WQ.front().cpu >= NUM_CPUS) || (WQ.front().event_cycle > current_core_cycle[WQ.front().cpu]))
            return;

        // handle the oldest entry
        PACKET &handle_pkt = WQ.front();

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
        }
        else // MISS
        {
            DP ( if (warmup_complete[handle_pkt.cpu]) {
                    std::cout << "[" << NAME << "] " << __func__ << " type: " << +handle_pkt.type << " miss";
                    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << handle_pkt.address;
                    std::cout << " full_addr: " << handle_pkt.full_addr << std::dec;
                    std::cout << " cycle: " << handle_pkt.event_cycle << std::endl; });

            bool success;
            if (cache_type == IS_L1D) {
                success = readlike_miss(handle_pkt);
            }
            else {
                // find victim
                way = find_victim(handle_pkt.cpu, handle_pkt.instr_id, set, &block.data()[set*NUM_WAY], handle_pkt.ip, handle_pkt.full_addr, handle_pkt.type);

                success = filllike_miss(set, way, handle_pkt);
            }

            if (!success)
                return;
        }

        // remove this entry from WQ
        writes_available_this_cycle--;
        WQ.pop_front();
    }
}

void CACHE::handle_read()
{
    while (reads_available_this_cycle > 0) {

        if (!RQ.has_ready() || (RQ.front().cpu >= NUM_CPUS) || (RQ.front().event_cycle > current_core_cycle[RQ.front().cpu]))
            return;

        // handle the oldest entry
        PACKET &handle_pkt = RQ.front();

        uint32_t set = get_set(handle_pkt.address);
        uint32_t way = get_way(handle_pkt.address, set);

        if (way < NUM_WAY) // HIT
        {
            readlike_hit(set, way, handle_pkt);
        }
        else {
            bool success = readlike_miss(handle_pkt);
            if (!success)
                return;
        }

        // remove this entry from RQ
        RQ.pop_front();
        reads_available_this_cycle--;
    }
}

void CACHE::handle_prefetch()
{
    while (reads_available_this_cycle > 0)
    {
        if (!PQ.has_ready() || (PQ.front().cpu >= NUM_CPUS) || (PQ.front().event_cycle > current_core_cycle[PQ.front().cpu]))
            return;

        // handle the oldest entry
        PACKET &handle_pkt = PQ.front();

        uint32_t set = get_set(handle_pkt.address);
        uint32_t way = get_way(handle_pkt.address, set);

        if (way < NUM_WAY) // HIT
        {
            readlike_hit(set, way, handle_pkt);
        }
        else {
            bool success = readlike_miss(handle_pkt);
            if (!success)
                return;
        }

        // remove this entry from PQ
        PQ.pop_front();
        reads_available_this_cycle--;
    }
}

void CACHE::readlike_hit(std::size_t set, std::size_t way, PACKET &handle_pkt)
{
    BLOCK &hit_block = block[set*NUM_WAY + way];

    handle_pkt.data = hit_block.data;

    // update prefetcher on load instruction
    if (handle_pkt.type == LOAD || (handle_pkt.type == PREFETCH && handle_pkt.pf_origin_level < fill_level))
    {
        if(cache_type == IS_L1I)
            l1i_prefetcher_cache_operate(handle_pkt.cpu, handle_pkt.ip, 1, hit_block.prefetch);
        if (cache_type == IS_L1D)
            l1d_prefetcher_operate(handle_pkt.full_v_addr, handle_pkt.full_addr, handle_pkt.ip, 1, handle_pkt.type);
        else if (cache_type == IS_L2C)
            l2c_prefetcher_operate(handle_pkt.v_address << LOG2_BLOCK_SIZE, hit_block.address << LOG2_BLOCK_SIZE, handle_pkt.ip, 1, handle_pkt.type, 0);
        else if (cache_type == IS_LLC)
        {
            cpu = handle_pkt.cpu;
            llc_prefetcher_operate(handle_pkt.v_address << LOG2_BLOCK_SIZE, hit_block.address << LOG2_BLOCK_SIZE, handle_pkt.ip, 1, handle_pkt.type, 0);
            cpu = 0;
        }
    }

    // update replacement policy
    update_replacement_state(handle_pkt.cpu, set, way, hit_block.full_addr, handle_pkt.ip, 0, handle_pkt.type, 1);

    // COLLECT STATS
    sim_hit[handle_pkt.cpu][handle_pkt.type]++;
    sim_access[handle_pkt.cpu][handle_pkt.type]++;

    for (auto ret : handle_pkt.to_return)
        ret->return_data(&handle_pkt);

    // update prefetch stats and reset prefetch bit
    if (hit_block.prefetch) {
        pf_useful++;
        hit_block.prefetch = 0;
    }
    hit_block.used = 1;
}

bool CACHE::readlike_miss(PACKET &handle_pkt)
{
    // check mshr
    auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(handle_pkt.address));
    bool mshr_full = std::all_of(MSHR.begin(), MSHR.end(), is_valid<PACKET>());

    if (mshr_entry != MSHR.end()) // miss already inflight
    {
        // update fill location
        mshr_entry->fill_level = std::min(mshr_entry->fill_level, handle_pkt.fill_level);

        packet_dep_merge(mshr_entry->lq_index_depend_on_me, handle_pkt.lq_index_depend_on_me);
        packet_dep_merge(mshr_entry->sq_index_depend_on_me, handle_pkt.sq_index_depend_on_me);
        packet_dep_merge(mshr_entry->instr_depend_on_me, handle_pkt.instr_depend_on_me);
        packet_dep_merge(mshr_entry->to_return, handle_pkt.to_return);

        if (mshr_entry->type == PREFETCH && handle_pkt.type != PREFETCH)
        {
            uint8_t  prior_returned = mshr_entry->returned;
            uint64_t prior_event_cycle = mshr_entry->event_cycle;
            *mshr_entry = handle_pkt;

            // in case request is already returned, we should keep event_cycle and retunred variables
            mshr_entry->returned = prior_returned;
            mshr_entry->event_cycle = prior_event_cycle;
        }
    }
    else
    {
        if (mshr_full) // not enough MSHR resource
            return false; // TODO should we allow prefetches anyway if they will not be filled to this level?

        // check to make sure the lower level RQ has room for this read miss
        if (cache_type == IS_LLC && lower_level->get_occupancy(1, handle_pkt.address) == lower_level->get_size(1, handle_pkt.address))
            return false;

        // Non-LLC prefetches are prefetch requests to lower level
        if (cache_type != IS_LLC && handle_pkt.type == PREFETCH && lower_level->get_occupancy(3, handle_pkt.address) == lower_level->get_size(3, handle_pkt.address))
            return false;

        // Allocate an MSHR
        if (handle_pkt.fill_level <= fill_level)
        {
            auto it = std::find_if_not(MSHR.begin(), MSHR.end(), is_valid<PACKET>());
            assert(it != std::end(MSHR));
            *it = handle_pkt;
            it->returned = INFLIGHT;
            it->cycle_enqueued = current_core_cycle[handle_pkt.cpu];
        }

        // Send to the lower level
        if (cache_type != IS_STLB)
        {
            if (handle_pkt.fill_level <= fill_level)
                handle_pkt.to_return = {this};
            else
                handle_pkt.to_return.clear();

            if (handle_pkt.type == PREFETCH && cache_type != IS_LLC)
                lower_level->add_pq(&handle_pkt);
            else
                lower_level->add_rq(&handle_pkt);
        }
        else
        {
            // TODO: need to differentiate page table walk and actual swap
            handle_pkt.data = vmem.va_to_pa(handle_pkt.cpu, handle_pkt.full_addr) >> LOG2_PAGE_SIZE;
            handle_pkt.event_cycle = current_core_cycle[handle_pkt.cpu];
            return_data(&handle_pkt);
        }
    }

    // update prefetcher on load instructions and prefetches from upper levels
    if (handle_pkt.type == LOAD || (handle_pkt.type == PREFETCH && handle_pkt.pf_origin_level < fill_level))
    {
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

    return true;
}

bool CACHE::filllike_miss(std::size_t set, std::size_t way, PACKET &handle_pkt)
{
    bool bypass = (way == NUM_WAY);
#ifndef LLC_BYPASS
    assert(!bypass);
#endif
    assert(handle_pkt.type != WRITEBACK || !bypass);

    BLOCK &fill_block = block[set*NUM_WAY + way];
    bool evicting_dirty = !bypass && (lower_level != NULL) && fill_block.dirty;
    auto evicting_l1i_v_addr = bypass ? 0 : fill_block.ip;
    auto evicting_address = bypass ? 0 : fill_block.address;

    // is this dirty?
    if (evicting_dirty && (lower_level->get_occupancy(2, fill_block.address) == lower_level->get_size(2, fill_block.address))) {

        // lower level WQ is full, cannot replace this victim
        lower_level->increment_WQ_FULL(fill_block.address);

        DP ( if (warmup_complete[handle_pkt.cpu]) {
                std::cout << "[" << NAME << "] " << __func__ << " ceasing write. ";
                std::cout << " Lower level wq is full!" << " fill_addr: " << std::hex << handle_pkt.address;
                std::cout << " victim_addr: " << fill_block.tag << std::dec << std::endl; });
        return false;
    }

    if (!bypass)
    {
        if (evicting_dirty) {
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

            auto result = lower_level->add_wq(&writeback_packet);
            if (result == -2)
                return false;
        }

        assert(cache_type != IS_ITLB || handle_pkt.data != 0);
        assert(cache_type != IS_DTLB || handle_pkt.data != 0);
        assert(cache_type != IS_STLB || handle_pkt.data != 0);

        if (fill_block.prefetch && !fill_block.used)
            pf_useless++;

        if (handle_pkt.type == PREFETCH)
            pf_fill++;

        auto lru = fill_block.lru; // preserve LRU state
        fill_block = handle_pkt; // fill cache
        fill_block.lru = lru;

        if (handle_pkt.type == WRITEBACK || (handle_pkt.type == RFO && cache_type == IS_L1D))
            fill_block.dirty = 1;
    }

    if(warmup_complete[handle_pkt.cpu] && (handle_pkt.cycle_enqueued != 0))
        total_miss_latency += current_core_cycle[handle_pkt.cpu] - handle_pkt.cycle_enqueued;

    // update prefetcher
    if (cache_type == IS_L1I)
        l1i_prefetcher_cache_fill(handle_pkt.cpu, handle_pkt.ip & ~(BLOCK_SIZE-1), set, way, handle_pkt.type == PREFETCH, evicting_l1i_v_addr & ~(BLOCK_SIZE-1));
    if (cache_type == IS_L1D)
        l1d_prefetcher_cache_fill(handle_pkt.full_v_addr, handle_pkt.full_addr, set, way, handle_pkt.type == PREFETCH, evicting_address << LOG2_BLOCK_SIZE, handle_pkt.pf_metadata);
    if  (cache_type == IS_L2C)
        handle_pkt.pf_metadata = l2c_prefetcher_cache_fill(handle_pkt.v_address << LOG2_BLOCK_SIZE, handle_pkt.address << LOG2_BLOCK_SIZE, set, way, handle_pkt.type == PREFETCH, evicting_address << LOG2_BLOCK_SIZE, handle_pkt.pf_metadata);
    if (cache_type == IS_LLC)
    {
        cpu = handle_pkt.cpu;
        handle_pkt.pf_metadata = llc_prefetcher_cache_fill(handle_pkt.v_address << LOG2_BLOCK_SIZE, handle_pkt.address << LOG2_BLOCK_SIZE, set, way, handle_pkt.type == PREFETCH, evicting_address << LOG2_BLOCK_SIZE, handle_pkt.pf_metadata);
        cpu = 0;
    }

    // update replacement policy
    update_replacement_state(handle_pkt.cpu, set, way, handle_pkt.full_addr, handle_pkt.ip, 0, handle_pkt.type, 0);

    // COLLECT STATS
    sim_miss[handle_pkt.cpu][handle_pkt.type]++;
    sim_access[handle_pkt.cpu][handle_pkt.type]++;

    return true;
}

void CACHE::operate()
{
  operate_writes();
  operate_reads();
}

void CACHE::operate_writes()
{
    // perform all writes
    writes_available_this_cycle = MAX_WRITE;
    handle_fill();
    handle_writeback();

    WQ.operate();
}

void CACHE::operate_reads()
{
    // perform all reads
    reads_available_this_cycle = MAX_READ;
    handle_read();
    va_translate_prefetches();
    handle_prefetch();

    RQ.operate();
    PQ.operate();
    VAPQ.operate();
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
    assert(packet->address != 0);
    RQ_ACCESS++;

    // check for the latest writebacks in the write queue
    champsim::delay_queue<PACKET>::iterator found_wq;
    if (cache_type == IS_L1D) {
        found_wq = std::find_if(WQ.begin(), WQ.end(), eq_full_addr<PACKET>(packet->full_addr));
    }
    else {
        found_wq = std::find_if(WQ.begin(), WQ.end(), eq_addr<PACKET>(packet->address));
    }

    if (found_wq != WQ.end()) {

        packet->data = found_wq->data;
        for (auto ret : packet->to_return)
            ret->return_data(packet);

        WQ_FORWARD++;
        return -1;
    }

    // check for duplicates in the read queue
    auto found_rq = std::find_if(RQ.begin(), RQ.end(), eq_addr<PACKET>(packet->address));
    if (found_rq != RQ.end()) {

        packet_dep_merge(found_rq->lq_index_depend_on_me, packet->lq_index_depend_on_me);
        packet_dep_merge(found_rq->sq_index_depend_on_me, packet->sq_index_depend_on_me);
        packet_dep_merge(found_rq->instr_depend_on_me, packet->instr_depend_on_me);
        packet_dep_merge(found_rq->to_return, packet->to_return);

        RQ_MERGED++;

        return 1; // merged index
    }

    // check occupancy
    if (RQ.full()) {
        RQ_FULL++;

        return -2; // cannot handle this request
    }

    // if there is no duplicate, add it to RQ
    if (warmup_complete[cpu])
        RQ.push_back(*packet);
    else
        RQ.push_back_ready(*packet);

    DP ( if (warmup_complete[packet->cpu]) {
            std::cout << "[" << NAME << "_RQ] " <<  __func__ << " instr_id: " << packet->instr_id << " address: " << std::hex << packet->address;
            std::cout << " full_addr: " << packet->full_addr << std::dec << " type: " << +packet->type << " occupancy: " << RQ.occupancy() << std::endl; })

    RQ_TO_CACHE++;
    return -1;
}

int CACHE::add_wq(PACKET *packet)
{
    WQ_ACCESS++;

    // check for duplicates in the write queue
    champsim::delay_queue<PACKET>::iterator found_wq;
    if (cache_type == IS_L1D) {
        found_wq = std::find_if(WQ.begin(), WQ.end(), eq_full_addr<PACKET>(packet->full_addr));
    }
    else {
        found_wq = std::find_if(WQ.begin(), WQ.end(), eq_addr<PACKET>(packet->address));
    }

    if (found_wq != WQ.end()) {

        WQ_MERGED++;
        return 1; // merged index
    }

    // Check for room in the queue
    if (WQ.full())
    {
        ++WQ_FULL;
        return -2;
    }

    // if there is no duplicate, add it to the write queue
    if (warmup_complete[cpu])
        WQ.push_back(*packet);
    else
        WQ.push_back_ready(*packet);

    DP (if (warmup_complete[WQ.entry[index].cpu]) {
            std::cout << "[" << NAME << "_WQ] " <<  __func__ << " instr_id: " << packet->instr_id << " address: " << std::hex << packet->address;
            std::cout << " full_addr: " << packet->full_addr << std::dec << " occupancy: " << WQ.occupancy();
            std::cout << " data: " << std::hex << packet->data << std::dec << std::endl; })

    WQ_TO_CACHE++;
    WQ_ACCESS++;

    return -1;
}

int CACHE::prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr, int pf_fill_level, uint32_t prefetch_metadata)
{
    pf_requested++;

    if (!PQ.full()) {
        if ((base_addr>>LOG2_PAGE_SIZE) == (pf_addr>>LOG2_PAGE_SIZE)) {
            
            PACKET pf_packet;
            pf_packet.fill_level = pf_fill_level;
	    pf_packet.pf_origin_level = fill_level;
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
    if (!PQ.full()) {
        if ((base_addr>>LOG2_PAGE_SIZE) == (pf_addr>>LOG2_PAGE_SIZE)) {
            
            PACKET pf_packet;
            pf_packet.fill_level = pf_fill_level;
	    pf_packet.pf_origin_level = fill_level;
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
            //pf_packet.delta = delta;
            //pf_packet.depth = depth;
            //pf_packet.signature = signature;
            //pf_packet.confidence = confidence;
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
  if(!VAPQ.full())
    {
      // generate new prefetch request packet
      PACKET pf_packet;
      pf_packet.fill_level = pf_fill_level;
      pf_packet.pf_origin_level = fill_level;
      pf_packet.pf_metadata = prefetch_metadata;
      pf_packet.cpu = cpu;
      pf_packet.v_address = pf_addr >> LOG2_BLOCK_SIZE;
      pf_packet.address = pf_addr >> LOG2_BLOCK_SIZE; // make address == v_address before translation just so we can use VAPQ's check_queue() function
	pf_packet.full_v_addr = pf_addr;
      pf_packet.full_addr = pf_addr;
      pf_packet.ip = ip;
      pf_packet.type = PREFETCH;
      pf_packet.event_cycle = 0;
      pf_packet.to_return = {this};

      auto vapq_entry = std::find_if(VAPQ.begin(), VAPQ.end(), eq_addr<PACKET>(pf_addr >> LOG2_BLOCK_SIZE));
      if(vapq_entry != VAPQ.end())
	{
	  // there's already a VA prefetch to this cache line
	  return 1;
	}

      // add the packet to the virtual address space prefetching queue
      VAPQ.push_back(pf_packet);
      return 1;
    }

  return 0;
}

void CACHE::va_translate_prefetches()
{
    // TEMPORARY SOLUTION: mark prefetches as translated after a fixed latency
    if (!PQ.full() && VAPQ.has_ready())
    {
        VAPQ.front().full_addr = vmem.va_to_pa(cpu, VAPQ.front().full_v_addr);
        VAPQ.front().address   = VAPQ.front().full_addr >> LOG2_BLOCK_SIZE;

        // move the translated prefetch over to the regular PQ
        add_pq(&VAPQ.front());

        // remove the prefetch from the VAPQ
        VAPQ.pop_front();
    }
}

int CACHE::add_pq(PACKET *packet)
{
    assert(packet->address != 0);
    PQ_ACCESS++;

    // check for the latest wirtebacks in the write queue
    champsim::delay_queue<PACKET>::iterator found_wq;
    if (cache_type == IS_L1D) {
        found_wq = std::find_if(WQ.begin(), WQ.end(), eq_full_addr<PACKET>(packet->full_addr));
    }
    else {
        found_wq = std::find_if(WQ.begin(), WQ.end(), eq_addr<PACKET>(packet->address));
    }

    if (found_wq != WQ.end()) {
        
        packet->data = found_wq->data;
        for (auto ret : packet->to_return)
            ret->return_data(packet);

        WQ_FORWARD++;
        return -1;
    }

    // check for duplicates in the PQ
    auto found = std::find_if(PQ.begin(), PQ.end(), eq_addr<PACKET>(packet->address));
    if (found != PQ.end())
    {
        found->fill_level = std::min(found->fill_level, packet->fill_level);
        packet_dep_merge(found->to_return, packet->to_return);

        PQ_MERGED++;
        return 1; // merged index
    }

    // check occupancy
    if (PQ.full()) {
        PQ_FULL++;

        DP ( if (warmup_complete[packet->cpu]) {
        cout << "[" << NAME << "] cannot process add_pq since it is full" << endl; });
        return -2; // cannot handle this request
    }

    // if there is no duplicate, add it to PQ
    if (warmup_complete[cpu])
        PQ.push_back(*packet);
    else
        PQ.push_back_ready(*packet);

    DP ( if (warmup_complete[packet->cpu]) {
            std::cout << "[" << NAME << "_PQ] " <<  __func__ << " instr_id: " << packet->instr_id << " address: " << std::hex << packet->address;
            std::cout << " full_addr: " << packet->full_addr << std::dec << " type: " << +packet->type << " occupancy: " << PQ.occupancy() << std::endl; })

    PQ_TO_CACHE++;
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
    if (warmup_complete[cpu])
    {
        if (mshr_entry->event_cycle < current_core_cycle[packet->cpu])
            mshr_entry->event_cycle = current_core_cycle[packet->cpu] + FILL_LATENCY;
        else
            mshr_entry->event_cycle += FILL_LATENCY;
    }
    else
    {
        mshr_entry->event_cycle = current_core_cycle[cpu];
    }

    DP (if (warmup_complete[packet->cpu]) {
            std::cout << "[" << NAME << "_MSHR] " <<  __func__ << " instr_id: " << mshr_entry->instr_id;
            std::cout << " address: " << std::hex << mshr_entry->address << " full_addr: " << mshr_entry->full_addr;
            std::cout << " data: " << mshr_entry->data << std::dec;
            std::cout << " index: " << std::distance(MSHR.begin(), mshr_entry) << " occupancy: " << get_occupancy(0,0);
            std::cout << " event: " << mshr_entry->event_cycle << " current: " << current_core_cycle[packet->cpu] << std::endl; });

    MSHR.sort(min_fill_index());
}

uint32_t CACHE::get_occupancy(uint8_t queue_type, uint64_t address)
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

uint32_t CACHE::get_size(uint8_t queue_type, uint64_t address)
{
    if (queue_type == 0)
        return MSHR_SIZE;
    else if (queue_type == 1)
        return RQ.size();
    else if (queue_type == 2)
        return WQ.size();
    else if (queue_type == 3)
        return PQ.size();

    return 0;
}

void CACHE::increment_WQ_FULL(uint64_t address)
{
    WQ_FULL++;
}

