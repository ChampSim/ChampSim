#ifndef PTW_H
#define PTW_H

#include <map>

#define IS_PSCL5 11
#define IS_PSCL4 12
#define IS_PSCL3 13
#define IS_PSCL2 14

#define NUM_ENTRIES_PER_PAGE 512

#define IS_PTL1 1
#define IS_PTL2 2
#define IS_PTL3 3
#define IS_PTL4 4
#define IS_PTL5 5

//Virtual Address: 57 bit (9+9+9+9+9+12), rest MSB bits will be used to generate a unique VA per CPU.
//PTL5->PTL4->PTL3->PTL2->PTL1->PFN

class PageTablePage
{
    public:
        PageTablePage* entry[NUM_ENTRIES_PER_PAGE] = {};
        uint64_t next_level_base_addr[NUM_ENTRIES_PER_PAGE];

        PageTablePage()
        {
            std::fill(std::begin(next_level_base_addr), std::end(next_level_base_addr), UINT64_MAX);
        }

        ~PageTablePage()
        {
            for(int i = 0; i < NUM_ENTRIES_PER_PAGE; i++)
            {
                if(entry[i] != NULL)
                    delete(entry[i]);
            }
        }
};

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

        PageTablePage *L5 = new PageTablePage(); //CR3 register points to the base of this page.
        uint64_t CR3_addr = map_translation_page(0,0); //This address will not have page offset bits.

        PageTableWalker(string v1, uint32_t v2, uint32_t v3, uint32_t v4, uint32_t v5, uint32_t v6, uint32_t v7, uint32_t v8, uint32_t v9, uint32_t v10, uint32_t v11, uint32_t v12, uint32_t v13, unsigned latency)
            : NAME(v1),
            MSHR_SIZE(v11), MAX_READ(v12), MAX_FILL(v13),
            RQ{v10, latency},
            PSCL5{"PSCL5", 12+9+9+9+9, v2, v3}, //Translation from L5->L4
            PSCL4{"PSCL4", 12+9+9+9, v4, v5}, //Translation from L5->L3
            PSCL3{"PSCL3", 12+9+9, v6, v7}, //Translation from L5->L2
            PSCL2{"PSCL2", 12+9, v8, v9}  //Translation from L5->L1
        {
        }

        ~PageTableWalker()
        {
            delete L5;
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

        uint64_t get_offset(uint64_t address, uint8_t pt_level);
        void     handle_page_fault(PageTablePage* page, PACKET *packet, uint8_t pt_level);

        uint64_t map_translation_page(uint64_t full_virtual_address, uint8_t level),
                 map_data_page(uint64_t instr_id, uint64_t full_virtual_address);

        void write_translation_page(uint64_t next_level_base_addr, PACKET *packet, uint8_t pt_level);
};

#endif
