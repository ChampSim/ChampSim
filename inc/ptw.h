#ifndef PTW_H
#define PTW_H

#include <map>
#include <optional>

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

    public:
        const std::size_t level;
        PagingStructureCache(string v1, uint8_t v2, uint32_t v3, uint32_t v4) : NAME(v1), NUM_SET(v3), NUM_WAY(v4), level(v2) {}

        std::optional<uint64_t> check_hit(uint64_t address);
        void fill_cache(uint64_t next_level_base_addr, PACKET *packet);
};

class PageTableWalker : public MemoryRequestConsumer, public MemoryRequestProducer
{
    public:
        const string NAME;
        const uint32_t cpu;
        const uint32_t MSHR_SIZE, MAX_READ, MAX_FILL;

        champsim::delay_queue<PACKET> RQ;

        std::list<PACKET> MSHR;

        uint64_t total_miss_latency = 0;

        PagingStructureCache PSCL5, PSCL4, PSCL3, PSCL2;

        const uint64_t CR3_addr;
        std::map<std::pair<uint64_t, std::size_t>, uint64_t> page_table;

        PageTableWalker(string v1, uint32_t cpu, uint32_t v2, uint32_t v3, uint32_t v4, uint32_t v5, uint32_t v6, uint32_t v7, uint32_t v8, uint32_t v9, uint32_t v10, uint32_t v11, uint32_t v12, uint32_t v13, unsigned latency);

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
};

#endif
