/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BLOCK_H
#define BLOCK_H

#include <algorithm>
#include <vector>

#include "champsim_constants.h"
#include "circular_buffer.hpp"
#include "instruction.h"

class MemoryRequestProducer;
class LSQ_ENTRY;

// message packet
class PACKET
{
public:
  bool scheduled = false;

  uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()}, type = 0, fill_level = 0, pf_origin_level = 0;

  uint32_t pf_metadata;
  uint32_t cpu = NUM_CPUS;

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
void packet_dep_merge(LIST& dest, LIST& src)
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
  uint64_t instr_id = 0, producer_id = std::numeric_limits<uint64_t>::max(), virtual_address = 0, physical_address = 0, ip = 0, event_cycle = 0;

  champsim::circular_buffer<ooo_model_instr>::iterator rob_index;

  uint8_t translated = 0, fetched = 0, asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};
};

template <>
struct is_valid<LSQ_ENTRY> {
  bool operator()(const LSQ_ENTRY& test) { return test.virtual_address != 0; }
};

#endif
