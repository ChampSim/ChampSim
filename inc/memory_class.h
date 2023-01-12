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

#ifndef MEMORY_CLASS_H
#define MEMORY_CLASS_H

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <vector>

#include "util.h"

enum access_type {
  LOAD = 0,
  RFO,
  PREFETCH,
  WRITE,
  TRANSLATION,
  NUM_TYPES,
};

class MemoryRequestProducer;
struct ooo_model_instr;

// message packet
class PACKET
{
public:
  bool scheduled = false;
  bool forward_checked = false;
  bool translate_issued = false;
  bool prefetch_from_this = false;
  bool fill_this_level = false;
  bool is_translated = true;

  uint8_t type = 0;
  uint16_t asid = std::numeric_limits<uint16_t>::max();

  uint32_t pf_metadata = 0;
  uint32_t cpu = std::numeric_limits<uint32_t>::max();

  uint64_t address = 0, v_address = 0, data = 0, instr_id = 0, ip = 0, event_cycle = std::numeric_limits<uint64_t>::max(), cycle_enqueued = 0;

  std::vector<std::reference_wrapper<ooo_model_instr>> instr_depend_on_me{};
  std::vector<MemoryRequestProducer*> to_return{};

  std::size_t translation_level = 0;
  std::size_t init_translation_level = 0;
};

template <>
struct is_valid<PACKET> {
  bool operator()(const PACKET& test) { return test.address != 0; }
};

class MemoryRequestConsumer
{
public:
  virtual bool add_rq(const PACKET& packet) = 0;
  virtual bool add_wq(const PACKET& packet) = 0;
  virtual bool add_pq(const PACKET& packet) = 0;
  virtual bool add_ptwq(const PACKET& packet) = 0;
  virtual std::size_t get_occupancy(uint8_t queue_type, uint64_t address) = 0;
  virtual std::size_t get_size(uint8_t queue_type, uint64_t address) = 0;

  explicit MemoryRequestConsumer() {}
};

class MemoryRequestProducer
{
public:
  MemoryRequestConsumer* lower_level;
  virtual void return_data(const PACKET& packet) = 0;

protected:
  MemoryRequestProducer() {}
  explicit MemoryRequestProducer(MemoryRequestConsumer* ll) : lower_level(ll) {}
};

#endif
