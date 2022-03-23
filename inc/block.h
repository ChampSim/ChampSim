#ifndef BLOCK_H
#define BLOCK_H

#include <algorithm>
#include <vector>

#include "circular_buffer.hpp"
#include "instruction.h"

class MemoryRequestProducer;
struct LSQ_ENTRY;

// message packet
class PACKET
{
public:
  bool scheduled = false;
  bool forward_checked = false;

  uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()}, type = 0, fill_level = 0, pf_origin_level = 0;

  uint32_t pf_metadata = 0;
  uint32_t cpu = std::numeric_limits<uint32_t>::max();

  uint64_t address = 0, v_address = 0, data = 0, instr_id = 0, ip = 0, event_cycle = std::numeric_limits<uint64_t>::max(), cycle_enqueued = 0;

  std::vector<std::vector<LSQ_ENTRY>::iterator> lq_index_depend_on_me = {}, sq_index_depend_on_me = {};
  std::vector<champsim::circular_buffer<ooo_model_instr>::iterator> instr_depend_on_me;
  std::vector<MemoryRequestProducer*> to_return;

  uint8_t translation_level = 0, init_translation_level = 0;
};

template <>
struct is_valid<PACKET> {
  bool operator()(const PACKET& test) { return test.address != 0; }
};

template <typename LIST>
void packet_dep_merge(LIST& dest, const LIST& src)
{
  dest.reserve(std::size(dest) + std::size(src));
  auto middle = std::end(dest);
  dest.insert(middle, std::begin(src), std::end(src));
  std::inplace_merge(std::begin(dest), middle, std::end(dest));
  auto uniq_end = std::unique(std::begin(dest), std::end(dest));
  dest.erase(uniq_end, std::end(dest));
}

// load/store queue
struct LSQ_ENTRY {
  bool valid = false;
  uint64_t instr_id = 0;
  uint64_t virtual_address = 0;
  uint64_t ip = 0;
  uint64_t event_cycle = 0;

  champsim::circular_buffer<ooo_model_instr>::iterator rob_index;

  uint8_t translated = 0;
  uint8_t fetched = 0;
  uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};

  uint64_t physical_address = 0;
  uint64_t producer_id = std::numeric_limits<uint64_t>::max();
};

#endif
