#ifndef PTW_H
#define PTW_H

#include <cassert>
#include <deque>
#include <map>
#include <optional>
#include <string>

#include "memory_class.h"
#include "operable.h"
#include "vmem.h"

class PageTableWalker : public champsim::operable, public MemoryRequestConsumer, public MemoryRequestProducer
{
public:
  const std::string NAME;
  const uint32_t RQ_SIZE, MSHR_SIZE, MAX_READ, MAX_FILL;
  const uint64_t HIT_LATENCY;

  std::deque<PACKET> RQ;
  std::deque<PACKET> MSHR;

  uint64_t total_miss_latency = 0;

  std::vector<champsim::simple_lru_table<uint64_t>> pscl;
  VirtualMemory& vmem;

  const uint64_t CR3_addr;
  std::map<std::pair<uint64_t, std::size_t>, uint64_t> page_table;

  PageTableWalker(std::string v1, uint32_t cpu, double freq_scale, std::vector<champsim::simple_lru_table<uint64_t>>&& _pscl, uint32_t v10, uint32_t v11,
                  uint32_t v12, uint32_t v13, uint64_t latency, MemoryRequestConsumer* ll, VirtualMemory& _vmem);

  // functions
  bool add_rq(const PACKET& packet) override;
  bool add_wq(const PACKET&) override { assert(0); }
  bool add_pq(const PACKET&) override { assert(0); }

  void return_data(const PACKET& packet) override;
  void operate() override;

  bool handle_read(const PACKET& pkt);
  bool handle_fill(const PACKET& pkt);
  bool step_translation(uint64_t addr, uint8_t transl_level, const PACKET& source);

  uint32_t get_occupancy(uint8_t queue_type, uint64_t address) override;
  uint32_t get_size(uint8_t queue_type, uint64_t address) override;

  uint64_t get_shamt(uint8_t pt_level);

  void print_deadlock() override;
};

#endif
