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

#include <array>
#include <deque>
#include <limits>   // for numeric_limits
#include <optional> // for optional
#include <string>

#include "address.h"
#include "bandwidth.h"
#include "channel.h"
#include "msl/lru_table.h"
#include "operable.h"
#include "waitable.h"

class PageTableWalker : public champsim::modules::page_table_walker_module
{
  struct pscl_entry {
    champsim::address vaddr;
    champsim::address ptw_addr;
    std::size_t level;
    // The address space this cached walk step belongs to. A walker is
    // hardware owned by a consumer, not by an address space: entries are
    // asid-tagged so concurrent address spaces never hit each other's steps.
    champsim::origin::id_type asid = 0;
  };

  struct pscl_indexer {
    champsim::data::bits shamt;
    auto operator()(const pscl_entry& entry) const { return entry.vaddr.slice_upper(shamt); }
  };

  struct pscl_tagger {
    champsim::data::bits shamt;
    auto operator()(const pscl_entry& entry) const { return std::pair{entry.vaddr.slice_upper(shamt), entry.asid}; }
  };

  using pscl_type = champsim::msl::lru_table<pscl_entry, pscl_indexer, pscl_tagger>;
  using channel_type = champsim::modules::channel_module;
  using request_type = typename champsim::request;
  using response_type = typename champsim::response;

  struct mshr_type {
    champsim::address address{};
    champsim::address v_address{};
    champsim::waitable<champsim::address> data{};

    std::vector<uint64_t> instr_depend_on_me{};
    std::vector<std::deque<response_type>*> to_return{};

    uint32_t pf_metadata = 0;
    champsim::origin origin{};

    std::size_t translation_level = 0;

    mshr_type(const request_type& req, std::size_t level);
  };

  std::deque<mshr_type> MSHR;
  std::deque<mshr_type> finished;
  std::deque<mshr_type> completed;

  std::vector<channel_type*> upper_levels;
  channel_type* lower_level;

  std::optional<mshr_type> handle_read(const request_type& pkt, channel_type* ul);
  std::optional<mshr_type> handle_fill(const mshr_type& fill_mshr);
  std::optional<mshr_type> step_translation(const mshr_type& source);

  void finish_packet(const response_type& packet);

public:
  const std::string NAME;
  const uint32_t MSHR_SIZE;
  champsim::bandwidth::maximum_type MAX_READ, MAX_FILL;
  const champsim::chrono::clock::duration HIT_LATENCY;

  std::vector<pscl_type> pscl;
  champsim::modules::vmem_module* vmem;
  std::size_t pt_levels_;

  explicit PageTableWalker(champsim::modules::ModuleBuilder builder);

  long operate() final;
  long poll_cycle() final;

  void begin_phase(bool warmup) override;
  void print_deadlock() final;

private:
  bool warmup_ = true;
  unsigned log2_page_size_ = 12;

public:
  bool is_warmup() const { return warmup_; }
};

#endif
