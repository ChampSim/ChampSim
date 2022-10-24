#ifndef PTW_H
#define PTW_H

#include <cassert>
#include <deque>
#include <string>

#include "memory_class.h"
#include "operable.h"
#include "util.h"
#include "vmem.h"

class PageTableWalker : public champsim::operable, public MemoryRequestConsumer, public MemoryRequestProducer
{
  struct pscl_entry
  {
    uint16_t asid;
    uint64_t vaddr;
    uint64_t ptw_addr;
  };

  struct pscl_set
  {
    std::size_t shamt;
    auto operator()(const pscl_entry &entry) const
    {
      return entry.vaddr >> shamt;
    }
  };

  struct pscl_tag
  {
    std::size_t shamt;
    auto operator()(const pscl_entry &entry) const
    {
      return std::pair{entry.asid, entry.vaddr >> shamt};
    }
  };

  using pscl_type = champsim::lru_table<pscl_entry, pscl_set, pscl_tag>;

public:
  const std::string NAME;
  const uint32_t RQ_SIZE, MSHR_SIZE, MAX_READ, MAX_FILL;
  const uint64_t HIT_LATENCY;

  std::deque<PACKET> RQ;
  std::deque<PACKET> MSHR;

  uint64_t total_miss_latency = 0;

  std::vector<pscl_type> pscl;
  VirtualMemory& vmem;

  const uint64_t CR3_addr;

  PageTableWalker(std::string v1, uint32_t cpu, double freq_scale, std::vector<std::pair<std::size_t, std::size_t>> pscl_dims, uint32_t v10, uint32_t v11,
                  uint32_t v12, uint32_t v13, uint64_t latency, MemoryRequestConsumer* ll, VirtualMemory& _vmem);

  // functions
  bool add_rq(const PACKET& packet) override final;
  bool add_wq(const PACKET&) override final { assert(0); }
  bool add_pq(const PACKET&) override final { assert(0); }

  void return_data(const PACKET& packet) override final;
  void operate() override final;

  bool handle_read(const PACKET& pkt);
  bool handle_fill(const PACKET& pkt);
  bool step_translation(uint64_t addr, uint8_t transl_level, const PACKET& source);

  uint32_t get_occupancy(uint8_t queue_type, uint64_t address) override final;
  uint32_t get_size(uint8_t queue_type, uint64_t address) override final;

  uint64_t get_shamt(uint8_t pt_level);

  void print_deadlock() override final;
};

#endif
