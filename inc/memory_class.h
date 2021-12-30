#ifndef MEMORY_CLASS_H
#define MEMORY_CLASS_H

#include "champsim.h"
#include "block.h"

#include <limits>

// CACHE ACCESS TYPE
#define LOAD      0
#define RFO       1
#define PREFETCH  2
#define WRITEBACK 3
#define TRANSLATION 4
#define NUM_TYPES 5

// CACHE BLOCK
class BLOCK {
  public:
    bool valid = false,
         prefetch = false,
         dirty = false,
         polluting = false;

    uint64_t address = 0,
             full_addr = 0,
             v_address = 0,
             full_v_addr = 0,
             data = 0,
             ip = 0,
             cpu = 0,
             instr_id = 0,
             evicted_addr = 0;

    // replacement state
    uint32_t lru = std::numeric_limits<uint32_t>::max() >> 1;
};

class MemoryRequestConsumer
{
    public:
        /*
         * add_*q() return values:
         *
         * -2 : queue full
         * -1 : packet value forwarded, returned
         * 0  : packet merged
         * >0 : new queue occupancy
         *
         */
        virtual int  add_rq(PACKET *packet) = 0;
        virtual int  add_wq(PACKET *packet) = 0;
        virtual int  add_pq(PACKET *packet) = 0;
        virtual uint32_t get_occupancy(uint8_t queue_type, uint64_t address) = 0;
        virtual uint32_t get_size(uint8_t queue_type, uint64_t address) = 0;
};

class MemoryRequestProducer
{
    public:
        MemoryRequestConsumer *lower_level;
        virtual void return_data(PACKET *packet) = 0;
    protected:
        MemoryRequestProducer() {}
        explicit MemoryRequestProducer(MemoryRequestConsumer *ll) : lower_level(ll) {}
};

#endif

