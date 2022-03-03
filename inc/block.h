#ifndef BLOCK_H
#define BLOCK_H

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <vector>

#include "util.h"

class MemoryRequestProducer;
class ooo_model_instr;

// message packet
class PACKET
{
public:
  bool scheduled = false;

  uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()}, type = 0, fill_level = 0, pf_origin_level = 0;

  uint32_t pf_metadata = 0;
  uint32_t cpu = std::numeric_limits<uint32_t>::max();

  uint64_t address = 0, v_address = 0, data = 0, instr_id = 0, ip = 0, event_cycle = std::numeric_limits<uint64_t>::max(), cycle_enqueued = 0;

  std::vector<std::reference_wrapper<ooo_model_instr>> instr_depend_on_me;
  std::vector<MemoryRequestProducer*> to_return;

  uint8_t translation_level = 0, init_translation_level = 0;
};

template <>
struct is_valid<PACKET> {
  bool operator()(const PACKET& test) { return test.address != 0; }
};

#endif
