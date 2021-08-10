#ifndef PTW_H
#define PTW_H

#include <map>

#define NUM_ENTRIES_PER_PAGE 512

//Virtual Address: 57 bit (9+9+9+9+9+12), rest MSB bits will be used to generate a unique VA per CPU.
//PTL5->PTL4->PTL3->PTL2->PTL1->PFN

class PagingStructureCache
{
    struct block_t
    {
        bool valid = false;
        uint64_t address;
        uint64_t data;
        uint32_t lru = std::numeric_limits<uint32_t>::max() >> 1;
    };

    const string NAME;
    const uint32_t NUM_SET, NUM_WAY;
    std::vector<block_t> block{NUM_SET*NUM_WAY};
    const std::size_t shamt;

    public:
        PagingStructureCache(string v1, uint8_t v2, uint32_t v3, uint32_t v4) : NAME(v1), NUM_SET(v3), NUM_WAY(v4), shamt(v2) {}

        uint64_t check_hit(uint64_t address);
        void fill_cache(uint64_t next_level_base_addr, PACKET *packet);
};

class PageTableWalker : public MemoryRequestConsumer, public MemoryRequestProducer
{
    public:
        const string NAME;
        uint32_t cpu;

        const uint32_t MSHR_SIZE, MAX_READ, MAX_FILL;

        uint64_t next_translation_virtual_address = 0xf000000f00000000;

        champsim::delay_queue<PACKET> RQ;

        std::list<PACKET> MSHR;

        uint64_t total_miss_latency = 0;

        PagingStructureCache PSCL5, PSCL4, PSCL3, PSCL2;

        uint64_t CR3_addr = map_translation_page(0);
        std::map<std::pair<uint64_t, std::size_t>, uint64_t> page_table;

        PageTableWalker(string v1, uint32_t v2, uint32_t v3, uint32_t v4, uint32_t v5, uint32_t v6, uint32_t v7, uint32_t v8, uint32_t v9, uint32_t v10, uint32_t v11, uint32_t v12, uint32_t v13, unsigned latency)
            : NAME(v1),
            MSHR_SIZE(v11), MAX_READ(v12), MAX_FILL(v13),
            RQ{v10, latency},
            PSCL5{"PSCL5", LOG2_PAGE_SIZE+4*lg2(NUM_ENTRIES_PER_PAGE), v2, v3}, //Translation from L5->L4
            PSCL4{"PSCL4", LOG2_PAGE_SIZE+3*lg2(NUM_ENTRIES_PER_PAGE), v4, v5}, //Translation from L5->L3
            PSCL3{"PSCL3", LOG2_PAGE_SIZE+2*lg2(NUM_ENTRIES_PER_PAGE), v6, v7}, //Translation from L5->L2
            PSCL2{"PSCL2", LOG2_PAGE_SIZE+1*lg2(NUM_ENTRIES_PER_PAGE), v8, v9}  //Translation from L5->L1
        {
        }

        // functions
        int add_rq(PACKET *packet);
        int add_wq(PACKET *packet) { assert(0); }
        int add_pq(PACKET *packet) { assert(0); }

        void return_data(PACKET *packet),
             operate();

        void handle_read(), handle_fill();
        void increment_WQ_FULL(uint64_t address) {}

        uint32_t get_occupancy(uint8_t queue_type, uint64_t address),
                 get_size(uint8_t queue_type, uint64_t address);

        uint64_t get_shamt(uint8_t pt_level);

        uint64_t map_translation_page(uint64_t full_virtual_address),
                 map_data_page(uint64_t full_virtual_address);
};

#endif
