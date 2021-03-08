#ifndef PTW_H
#define PTW_H

#include "cache.h"
#include <map>

#define IS_PSCL5 11
#define IS_PSCL4 12
#define IS_PSCL3 13
#define IS_PSCL2 14

#define PSCL5_SET 1
#define PSCL5_WAY 2

#define PSCL4_SET 1
#define PSCL4_WAY 4

#define PSCL3_SET 2
#define PSCL3_WAY 4

#define PSCL2_SET 4
#define PSCL2_WAY 8

#define PTW_RQ_SIZE STLB_MSHR_SIZE
#define PTW_WQ_SIZE 0
#define PTW_MSHR_SIZE 5
#define PTW_PQ_SIZE 0

#define PTW_MAX_READ 1
#define PTW_MAX_FILL 2

#define MMU_CACHE_LATENCY 1

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
		PageTablePage* entry[NUM_ENTRIES_PER_PAGE];
		uint64_t next_level_base_addr[NUM_ENTRIES_PER_PAGE];

	PageTablePage()
	{
		for(int i = 0; i < NUM_ENTRIES_PER_PAGE; i++)
		{
			entry[i] = NULL;
			next_level_base_addr[i] = UINT64_MAX;
		}
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

class PageTableWalker : public MemoryRequestConsumer, public MemoryRequestProducer
{
	public:
		const string NAME;
		uint32_t cpu;
		uint8_t cache_type;

		uint8_t LATENCY;
	
		uint64_t next_translation_virtual_address = 0xf000000f00000000;

		std::map<uint64_t, uint64_t> page_table;

		champsim::delay_queue<PACKET> RQ{PTW_RQ_SIZE, LATENCY},
									  WQ{PTW_WQ_SIZE, LATENCY},
									  PQ{PTW_PQ_SIZE, LATENCY};

		std::list<PACKET> MSHR{PTW_MSHR_SIZE};

    uint64_t RQ_ACCESS = 0,
             RQ_MERGED = 0,
             RQ_FULL = 0,
             RQ_TO_CACHE = 0,
             PQ_ACCESS = 0,
             PQ_MERGED = 0,
             PQ_FULL = 0,
             PQ_TO_CACHE = 0,
             WQ_ACCESS = 0,
             WQ_MERGED = 0,
             WQ_FULL = 0,
             WQ_FORWARD = 0,
             WQ_TO_CACHE = 0;

    uint64_t total_miss_latency = 0;
  

	CACHE PSCL5{"PSCL5", PSCL5_SET, PSCL5_WAY, 0, 0, 0, 0, 0, 0}, //Translation from L5->L4
          PSCL4{"PSCL4", PSCL4_SET, PSCL4_WAY, 0, 0, 0, 0, 0, 0}, //Translation from L5->L3
          PSCL3{"PSCL3", PSCL3_SET, PSCL3_WAY, 0, 0, 0, 0, 0, 0}, //Translation from L5->L2
          PSCL2{"PSCL2", PSCL2_SET, PSCL2_WAY, 0, 0, 0, 0, 0, 0}; //Translation from L5->L1

    PageTablePage *L5; //CR3 register points to the base of this page.
    uint64_t CR3_addr; //This address will not have page offset bits.

	PageTableWalker(string v1) : NAME(v1)
	{
		CR3_addr = map_translation_page(0,0);
		L5 = new PageTablePage();

		PSCL5.fill_level = 0;
        PSCL4.fill_level = 0;
        PSCL3.fill_level = 0;
        PSCL2.fill_level = 0;
	}

	~PageTableWalker()
	{
		if(L5 != NULL)
			delete L5;
	}

	// functions
    int  add_rq(PACKET *packet),
         add_wq(PACKET *packet),
         add_pq(PACKET *packet);

    void return_data(PACKET *packet),
         operate(),
         increment_WQ_FULL(uint64_t address),
         fill_mmu_cache(CACHE &cache, uint64_t next_level_base_addr, PACKET *packet, uint8_t cache_type),
         add_mshr(PACKET *packet);

	void handle_read(),
		 handle_fill();

    uint32_t get_occupancy(uint8_t queue_type, uint64_t address),
             get_size(uint8_t queue_type, uint64_t address);

    uint64_t get_index(uint64_t address, uint8_t cache_type),
             check_hit(CACHE &cache, uint64_t address, uint8_t type),
             get_offset(uint64_t address, uint8_t pt_level);
	void     handle_page_fault(PageTablePage* page, PACKET *packet, uint8_t pt_level);

    uint64_t map_translation_page(uint64_t full_virtual_address, uint8_t level),
         map_data_page(uint64_t instr_id, uint64_t full_virtual_address);

    void write_translation_page(uint64_t next_level_base_addr, PACKET *packet, uint8_t pt_level);
};

#endif	
