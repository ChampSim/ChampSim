#ifndef PTW_H
#define PTW_H

#include <cassert>
#include <deque>
#include <string>

#include "channel.h"
#include "operable.h"
#include "util.h"
#include "vmem.h"

class PageTableWalker : public champsim::operable
{
  struct pscl_entry {
    uint64_t vaddr;
    uint64_t ptw_addr;
    std::size_t level;
  };

  struct pscl_indexer {
    std::size_t shamt;
    auto operator()(const pscl_entry& entry) const { return entry.vaddr >> shamt; }
  };

  using pscl_type = champsim::lru_table<pscl_entry, pscl_indexer, pscl_indexer>;
  using channel_type = champsim::channel;
  using request_type = typename channel_type::request_type;
  using response_type = typename channel_type::response_type;
  using mshr_type = request_type;

  std::deque<mshr_type> MSHR;

  std::vector<channel_type*> upper_levels;
  channel_type* lower_level;

public:
  const std::string NAME;
  const uint32_t RQ_SIZE, MSHR_SIZE;
  const long int MAX_READ, MAX_FILL;
  const uint64_t HIT_LATENCY;

  uint64_t total_miss_latency = 0;

  std::vector<pscl_type> pscl;
  VirtualMemory& vmem;

  const uint64_t CR3_addr;

  PageTableWalker(std::string v1, uint32_t cpu, double freq_scale, std::vector<std::pair<std::size_t, std::size_t>> pscl_dims, uint32_t v10, uint32_t v11,
                  uint32_t v12, uint32_t v13, uint64_t latency, std::vector<channel_type*>&& ul, channel_type* ll, VirtualMemory& _vmem);

  void finish_packet(const response_type& packet);
  void operate() override final;

  bool handle_read(const request_type& pkt);
  bool handle_fill(const mshr_type& pkt);
  bool step_translation(uint64_t addr, std::size_t transl_level, const request_type& source);

  void print_deadlock() override final;
};

#endif
