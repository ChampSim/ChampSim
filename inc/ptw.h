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

class PagingStructureCache
{
	public:
		const string NAME;
		const uint32_t NUM_SET, NUM_WAY;
		std::vector<BLOCK> block{NUM_SET*NUM_WAY};
		uint8_t cache_type;

		PagingStructureCache(string v1, uint8_t v2, uint32_t v3, uint32_t v4) : NAME(v1), NUM_SET(v3), NUM_WAY(v4), cache_type(v2) {}

		uint32_t get_set(uint64_t address);
		uint64_t get_index(uint64_t address);
		uint64_t check_hit(uint64_t address);
		void fill_cache(uint64_t next_level_base_addr, PACKET *packet);
};

class PageTableWalker : public champsim::operable, public MemoryRequestConsumer, public MemoryRequestProducer
{
	public:
		const string NAME;
		uint32_t cpu;
		uint8_t cache_type;

		uint8_t LATENCY = 0;

		const uint32_t VAL_PSCL5_SET, VAL_PSCL5_WAY, VAL_PSCL4_SET, VAL_PSCL4_WAY, VAL_PSCL3_SET, VAL_PSCL3_WAY, VAL_PSCL2_SET, VAL_PSCL2_WAY;

		const uint32_t RQ_SIZE, WQ_SIZE = 0, PQ_SIZE = 0, MSHR_SIZE, MAX_READ, MAX_FILL;
		
		uint64_t next_translation_virtual_address = 0xf000000f00000000;

		champsim::delay_queue<PACKET> RQ{RQ_SIZE, LATENCY},
									  WQ{WQ_SIZE, LATENCY},
									  PQ{PQ_SIZE, LATENCY};

		std::list<PACKET> MSHR;

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
  

	PagingStructureCache PSCL5{"PSCL5", IS_PSCL5, VAL_PSCL5_SET, VAL_PSCL5_WAY}, //Translation from L5->L4
          PSCL4{"PSCL4", IS_PSCL4, VAL_PSCL4_SET, VAL_PSCL4_WAY}, //Translation from L5->L3
          PSCL3{"PSCL3", IS_PSCL3, VAL_PSCL3_SET, VAL_PSCL3_WAY}, //Translation from L5->L2
          PSCL2{"PSCL2", IS_PSCL2, VAL_PSCL2_SET, VAL_PSCL2_WAY}; //Translation from L5->L1

    PageTablePage *L5; //CR3 register points to the base of this page.
    uint64_t CR3_addr; //This address will not have page offset bits.

	PageTableWalker(string v1, uint32_t v2, uint32_t v3, uint32_t v4, uint32_t v5, uint32_t v6, uint32_t v7, uint32_t v8, uint32_t v9, uint32_t v10, uint32_t v11, uint32_t v12, uint32_t v13, MemoryRequestConsumer* ll) : champsim::operable(1), MemoryRequestProducer(ll), NAME(v1), VAL_PSCL5_SET(v2), VAL_PSCL5_WAY(v3),  VAL_PSCL4_SET(v4), VAL_PSCL4_WAY(v5), VAL_PSCL3_SET(v6), VAL_PSCL3_WAY(v7), VAL_PSCL2_SET(v8), VAL_PSCL2_WAY(v9), RQ_SIZE(v10), MSHR_SIZE(v11), MAX_READ(v12), MAX_FILL(v13) 
	{
		CR3_addr = map_translation_page(0,0);
		L5 = new PageTablePage();
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
         increment_WQ_FULL(uint64_t address),
         add_mshr(PACKET *packet);

    void operate() override;
    void begin_phase() override;
    void end_phase(unsigned cpu) override;

	void handle_read(),
		 handle_fill();

    uint32_t get_occupancy(uint8_t queue_type, uint64_t address),
             get_size(uint8_t queue_type, uint64_t address);

    uint64_t get_index(uint64_t address, uint8_t cache_type),
             get_offset(uint64_t address, uint8_t pt_level);
	void     handle_page_fault(PageTablePage* page, PACKET *packet, uint8_t pt_level);

    uint64_t map_translation_page(uint64_t full_virtual_address, uint8_t level),
         map_data_page(uint64_t instr_id, uint64_t full_virtual_address);

    void write_translation_page(uint64_t next_level_base_addr, PACKET *packet, uint8_t pt_level);
};

#endif	
