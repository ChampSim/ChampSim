#ifndef BLOCK_H
#define BLOCK_H

#include "champsim.h"
#include "instruction.h"
#include "circular_buffer.hpp"

#include <list>

class MemoryRequestProducer;
class LSQ_ENTRY;

// message packet
class PACKET {
  public:
    bool scheduled = false;

    uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()},
            type = 0,
            fill_level = 0,
            pf_origin_level = 0;

    int delta = 0,
        depth = 0,
        signature = 0,
        confidence = 0;

    uint32_t pf_metadata;

    std::list<LSQ_ENTRY*> lq_index_depend_on_me = {}, sq_index_depend_on_me = {};
    std::list<champsim::circular_buffer<ooo_model_instr>::iterator> instr_depend_on_me;

    uint32_t cpu = NUM_CPUS;

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

	uint8_t translation_level = 0, init_translation_level = 0; 
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

// load/store queue
struct LSQ_ENTRY {
    uint64_t instr_id = 0,
             producer_id = std::numeric_limits<uint64_t>::max(),
             virtual_address = 0,
             physical_address = 0,
             ip = 0,
             event_cycle = 0;

    ooo_model_instr* rob_index = NULL;

    uint8_t translated = 0,
            fetched = 0,
            asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};
};

template <>
class is_valid<LSQ_ENTRY>
{
    public:
        bool operator() (const LSQ_ENTRY &test)
        {
            return test.virtual_address != 0;
        }
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

