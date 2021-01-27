#ifndef BLOCK_H
#define BLOCK_H

#include "champsim.h"
#include "instruction.h"
#include "circular_buffer.hpp"

#include <list>

class MemoryRequestProducer;

// message packet
class PACKET {
  public:
    uint8_t scheduled = 0,
            translated = 0,
            fetched = 0,
            prefetched = 0;

    int fill_level = -1,
        pf_origin_level,
        rob_index = -1,
        producer = -1,
        delta = 0,
        depth = 0,
        signature = 0,
        confidence = 0;

    uint32_t pf_metadata;

    uint8_t  is_producer = 0, 
             returned = 0,
             asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()},
             type = 0;

    std::list<std::size_t> lq_index_depend_on_me = {}, sq_index_depend_on_me = {};
    std::list<champsim::circular_buffer<ooo_model_instr>::iterator> instr_depend_on_me;

    uint32_t cpu = NUM_CPUS,
             data_index = 0,
             lq_index = 0,
             sq_index = 0;

    uint64_t address = 0,
             full_addr = 0,
             v_address = 0,
             full_v_addr = 0,
             data = 0,
             instr_id = 0,
             ip = 0,
             event_cycle = std::numeric_limits<uint64_t>::max(),
             cycle_enqueued = 0;

    std::list<MemoryRequestProducer*> to_return;
};

template <typename LIST>
void packet_dep_merge(LIST &dest, LIST &src)
{
    if (src.empty())
        return;

    if (dest.empty())
    {
        dest = src;
        return;
    }

    auto s_begin = src.begin();
    auto s_end   = src.end();
    auto d_begin = dest.begin();
    auto d_end   = dest.end();

    while (s_begin != s_end && d_begin != d_end)
    {
        if (*s_begin > *d_begin)
        {
            ++d_begin;
        }
        else if (*s_begin == *d_begin)
        {
            ++s_begin;
        }
        else
        {
            dest.insert(d_begin, *s_begin);
            ++s_begin;
        }
    }

    dest.insert(d_begin, s_begin, s_end);
}

// packet queue
struct PACKET_QUEUE {
    string NAME;
    uint32_t SIZE;

    uint8_t  is_RQ = 0,
             is_WQ = 0,
             write_mode = 0;

    uint32_t cpu = 0,
             head = 0,
             tail = 0,
             occupancy = 0,
             num_returned = 0,
             next_schedule_index = 0,
             next_process_index = 0;

    uint64_t next_schedule_cycle = std::numeric_limits<uint64_t>::max(),
             next_process_cycle = std::numeric_limits<uint64_t>::max(),
             ACCESS = 0,
             FORWARD = 0,
             MERGED = 0,
             TO_CACHE = 0,
             ROW_BUFFER_HIT = 0,
             ROW_BUFFER_MISS = 0,
             FULL = 0;

    PACKET *entry;

    // constructor
    PACKET_QUEUE(string v1, uint32_t v2) : NAME(v1), SIZE(v2) {
        entry = new PACKET[SIZE];
    };

    PACKET_QUEUE() {}

    // destructor
    ~PACKET_QUEUE() {
        delete[] entry;
    };

    // functions
    int check_queue(PACKET* packet);
    void add_queue(PACKET* packet),
         remove_queue(PACKET* packet);
};

// load/store queue
struct LSQ_ENTRY {
    uint64_t instr_id = 0,
             producer_id = std::numeric_limits<uint64_t>::max(),
             virtual_address = 0,
             physical_address = 0,
             ip = 0,
             event_cycle = 0;

    uint32_t rob_index = 0, data_index = 0, sq_index = std::numeric_limits<uint32_t>::max();

    uint8_t translated = 0,
            fetched = 0,
            asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};
// forwarding_depend_on_me[ROB_SIZE];
    fastset
		forwarding_depend_on_me;
};

// reorder buffer
template <typename T>
struct CORE_BUFFER {
    const string NAME;
    const uint32_t SIZE;
    uint32_t head = 0, tail = 0, occupancy = 0;

    T *entry;

    // constructor
    CORE_BUFFER(string v1, uint32_t v2) : NAME(v1), SIZE(v2) {
        entry = new T[SIZE];
    };

    // destructor
    ~CORE_BUFFER() {
        delete[] entry;
    };
};

#endif

