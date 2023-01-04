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

public:
  const std::string NAME;
  const uint32_t RQ_SIZE, MSHR_SIZE;
  const long int MAX_READ, MAX_FILL;
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
  bool add_ptwq(const PACKET&) override final { assert(0); }

  void return_data(const PACKET& packet) override final;
  void operate() override final;

  bool handle_read(const PACKET& pkt);
  bool handle_fill(const PACKET& pkt);
  bool step_translation(uint64_t addr, std::size_t transl_level, const PACKET& source);

  std::size_t get_occupancy(uint8_t queue_type, uint64_t address) override final;
  std::size_t get_size(uint8_t queue_type, uint64_t address) override final;

  void print_deadlock() override final;
};

#endif
