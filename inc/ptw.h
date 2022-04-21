#ifndef PTW_H
#define PTW_H

#include <list>
#include <map>
#include <optional>
#include <string>

#include "delay_queue.hpp"
#include "memory_class.h"
#include "operable.h"
#include "vmem.h"

class PagingStructureCache
{
  struct block_t {
    bool valid = false;
    uint64_t address;
    uint64_t data;
    uint64_t last_used = 0;
  };

  const std::size_t shamt;
  const uint32_t NUM_SET, NUM_WAY;
  uint64_t access_count = 0;
  std::vector<block_t> block{NUM_SET * NUM_WAY};

public:
  const std::size_t level;
  PagingStructureCache(uint8_t v2, uint32_t v3, uint32_t v4, std::size_t shamt) : shamt(shamt), NUM_SET(v3), NUM_WAY(v4), level(v2) {}

  std::optional<uint64_t> check_hit(uint64_t address) const;
  void fill_cache(uint64_t next_level_paddr, uint64_t vaddr);
};

class PageTableWalker : public champsim::operable, public MemoryRequestConsumer, public MemoryRequestProducer
{
public:
  const std::string NAME;
  const uint32_t cpu;
  const uint32_t MSHR_SIZE, MAX_READ, MAX_FILL;

  champsim::delay_queue<PACKET> RQ;

  std::list<PACKET> MSHR;

  uint64_t total_miss_latency = 0;

  std::vector<PagingStructureCache> pscl;
  VirtualMemory &vmem;

  const uint64_t CR3_addr;
  std::map<std::pair<uint64_t, std::size_t>, uint64_t> page_table;

  PageTableWalker(std::string v1, uint32_t cpu, unsigned fill_level, PagingStructureCache &&pscl5, PagingStructureCache &&pscl4, PagingStructureCache &&pscl3, PagingStructureCache &&pscl2, uint32_t v10, uint32_t v11, uint32_t v12, uint32_t v13, unsigned latency, MemoryRequestConsumer* ll, VirtualMemory &_vmem);

  // functions
  bool add_rq(const PACKET& packet) override;
  bool add_wq(const PACKET& packet) override { assert(0); }
  bool add_pq(const PACKET& packet) override { assert(0); }

  void return_data(const PACKET& packet) override;
  void operate() override;

  void handle_read();
  void handle_fill();

  uint32_t get_occupancy(uint8_t queue_type, uint64_t address) override;
  uint32_t get_size(uint8_t queue_type, uint64_t address) override;

  uint64_t get_shamt(uint8_t pt_level);

  void print_deadlock() override;
};

#endif
