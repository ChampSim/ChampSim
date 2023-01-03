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

  struct mshr_type
  {
    uint64_t address = 0;
    uint64_t v_address = 0;
    uint64_t data = 0;

    std::vector<std::reference_wrapper<ooo_model_instr>> instr_depend_on_me{};
    std::vector<std::deque<response_type>*> to_return{};

    uint64_t event_cycle = std::numeric_limits<uint64_t>::max();
    uint32_t pf_metadata = 0;
    uint32_t cpu = std::numeric_limits<uint32_t>::max();
    uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};

    std::size_t translation_level = 0;

    mshr_type(request_type req, std::size_t level);
  };

  std::deque<mshr_type> MSHR;

  std::vector<channel_type*> upper_levels;
  channel_type* lower_level;

  bool handle_read(const request_type& pkt);
  bool handle_fill(const mshr_type& pkt);
  bool step_translation(const mshr_type& source);

  void finish_packet(const response_type& packet);

public:
  const std::string NAME;
  const uint32_t RQ_SIZE, MSHR_SIZE;
  const long int MAX_READ, MAX_FILL;
  const uint64_t HIT_LATENCY;

  std::vector<pscl_type> pscl;
  VirtualMemory& vmem;

  const uint64_t CR3_addr;

  PageTableWalker(std::string v1, uint32_t cpu, double freq_scale, std::vector<std::pair<std::size_t, std::size_t>> pscl_dims, uint32_t v10, uint32_t v11,
                  uint32_t v12, uint32_t v13, uint64_t latency, std::vector<channel_type*>&& ul, channel_type* ll, VirtualMemory& _vmem);

  void operate() override final;

  void print_deadlock() override final;
};

#endif
