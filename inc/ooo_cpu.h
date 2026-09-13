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

#ifndef OOO_CPU_H
#define OOO_CPU_H

#ifdef CHAMPSIM_MODULE
#define SET_ASIDE_CHAMPSIM_MODULE
#undef CHAMPSIM_MODULE
#endif

#include <array>
#include <bitset>
#include <cstdlib>
#include <deque>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "bandwidth.h"
#include "champsim.h"
#include "channel.h"
#include "core_stats.h"
#include "instruction.h"
#include "instruction_producer.h"
#include "modules.h"
#include "msl/lru_table.h"
#include "operable.h"
#include "register_allocator.h"
#include "util/to_underlying.h"

class CACHE;
class CacheBus
{
  using channel_type = champsim::modules::channel_module;
  using request_type = typename channel_type::request_type;
  using response_type = typename channel_type::response_type;

  channel_type* lower_level;

  friend class O3_CPU;

public:
  explicit CacheBus(champsim::modules::channel_module* ll) : lower_level(ll) {}
  bool issue_read(request_type packet);
  bool issue_write(request_type packet);
};

struct LSQ_ENTRY : champsim::program_ordered<LSQ_ENTRY> {
  champsim::address virtual_address{};
  champsim::address ip{};
  champsim::chrono::clock::time_point ready_time{champsim::chrono::clock::time_point::max()};

  champsim::origin origin{};
  bool fetch_issued = false;

  uint64_t producer_id = std::numeric_limits<uint64_t>::max();
  std::vector<std::reference_wrapper<std::optional<LSQ_ENTRY>>> lq_depend_on_me{};

  LSQ_ENTRY(champsim::address addr, champsim::program_ordered<LSQ_ENTRY>::id_type id, champsim::address ip, champsim::origin origin);
  void finish(ooo_model_instr& rob_entry) const;
  void finish(std::deque<ooo_model_instr>::iterator begin, std::deque<ooo_model_instr>::iterator end) const;
};

// cpu
class O3_CPU : public champsim::modules::core_module
{
public:
  // This core's consumer id (its "CPU number"): hardware-context identity
  // for provenance stamping, tables, stats, and phase tracking.

  // cycle
  champsim::chrono::clock::time_point begin_phase_time{};
  long long begin_phase_instr = 0;
  champsim::chrono::clock::time_point finish_phase_time{};
  long long finish_phase_instr = 0;

  // instruction
  long long num_retired = 0;

  using stats_type = cpu_stats;

  stats_type sim_stats{};

  // instruction buffer
  struct dib_shift {
    champsim::data::bits shamt;
    auto operator()(champsim::address val) const { return val.slice_upper(shamt); }
  };
  using dib_type = champsim::msl::lru_table<champsim::address, dib_shift, dib_shift>;
  dib_type DIB;

  // reorder buffer, load/store queue, register file
  std::deque<ooo_model_instr> IFETCH_BUFFER;
  std::deque<ooo_model_instr> DISPATCH_BUFFER;
  std::deque<ooo_model_instr> DECODE_BUFFER;
  std::deque<ooo_model_instr> ROB;
  std::deque<ooo_model_instr> DIB_HIT_BUFFER;

  std::vector<std::optional<LSQ_ENTRY>> LQ;
  std::deque<LSQ_ENTRY> SQ;

  // Constants
  const std::size_t IFETCH_BUFFER_SIZE, DISPATCH_BUFFER_SIZE, DECODE_BUFFER_SIZE, REGISTER_FILE_SIZE, ROB_SIZE, SQ_SIZE, DIB_HIT_BUFFER_SIZE;
  champsim::bandwidth::maximum_type FETCH_WIDTH, DECODE_WIDTH, DISPATCH_WIDTH, SCHEDULER_SIZE, EXEC_WIDTH, DIB_INORDER_WIDTH;
  champsim::bandwidth::maximum_type LQ_WIDTH, SQ_WIDTH;
  champsim::bandwidth::maximum_type RETIRE_WIDTH;
  champsim::chrono::clock::duration BRANCH_MISPREDICT_PENALTY;
  champsim::chrono::clock::duration DISPATCH_LATENCY;
  champsim::chrono::clock::duration DECODE_LATENCY;
  champsim::chrono::clock::duration SCHEDULING_LATENCY;
  champsim::chrono::clock::duration EXEC_LATENCY;
  champsim::chrono::clock::duration DIB_HIT_LATENCY;

  champsim::bandwidth::maximum_type L1I_BANDWIDTH, L1D_BANDWIDTH;

  RegisterAllocator reg_allocator{REGISTER_FILE_SIZE};

  // branch
  champsim::chrono::clock::time_point fetch_resume_time{};

  const long IN_QUEUE_SIZE;
  std::deque<ooo_model_instr> input_queue;

  CacheBus L1I_bus, L1D_bus;
  champsim::modules::cache_module* l1i;

  void initialize() final;
  long operate() final;
  void begin_phase(bool warmup) override;
  void end_phase(champsim::stat_report& out) override;

private:
  bool warmup_ = true;

public:
  bool is_warmup() const { return warmup_; }

  void push_instruction(ooo_model_instr instr) final;
  std::size_t instructions_requested() final;
  void initialize_instruction();
  long check_dib();
  long fetch_instruction();
  long promote_to_decode();
  long decode_instruction();
  long dispatch_instruction();
  long schedule_instruction();
  long execute_instruction();
  long operate_lsq();
  long complete_inflight_instruction();
  long handle_memory_return();
  long retire_rob();

  bool do_init_instruction(ooo_model_instr& instr);
  bool do_predict_branch(ooo_model_instr& instr);
  void do_check_dib(ooo_model_instr& instr);
  bool do_fetch_instruction(std::deque<ooo_model_instr>::iterator begin, std::deque<ooo_model_instr>::iterator end);
  void do_dib_update(const ooo_model_instr& instr);
  void do_scheduling(ooo_model_instr& instr);
  void do_execution(ooo_model_instr& instr);
  void do_memory_scheduling(ooo_model_instr& instr);
  void do_complete_execution(ooo_model_instr& instr);
  void do_sq_forward_to_lq(LSQ_ENTRY& sq_entry, LSQ_ENTRY& lq_entry);

  void do_finish_store(const LSQ_ENTRY& sq_entry);
  bool do_complete_store(const LSQ_ENTRY& sq_entry);
  bool execute_load(const LSQ_ENTRY& lq_entry);

  [[nodiscard]] uint64_t sim_instr() const final { return num_retired - begin_phase_instr; }
  [[nodiscard]] uint64_t sim_cycle() const final { return (current_time.time_since_epoch() / clock_period) - sim_stats.begin_cycles; }
  stats_type get_sim_stats() const final { return sim_stats; }

  void print_deadlock() final;

  std::vector<champsim::modules::branch_predictor*> branch_module_pimpl;
  std::vector<champsim::modules::btb*> btb_module_pimpl;
  std::vector<champsim::modules::instruction_producer*> instruction_producer_pimpl;

  void fill_from_producers();
  bool producers_eof() const final;
  std::vector<std::string> producer_descriptions() const final;

  // NOLINTBEGIN(readability-make-member-function-const): legacy modules use non-const hooks
  void impl_initialize_branch_predictor() const;
  void impl_last_branch_result(champsim::address ip, champsim::address target, bool taken, uint8_t branch_type) const;
  [[nodiscard]] bool impl_predict_branch(champsim::address ip, champsim::address predicted_target, bool always_taken, uint8_t branch_type) const;

  void impl_initialize_btb() const;
  void impl_update_btb(champsim::address ip, champsim::address predicted_target, bool taken, uint8_t branch_type) const;
  [[nodiscard]] std::pair<champsim::address, bool> impl_btb_prediction(champsim::address ip, uint8_t branch_type) const;
  // NOLINTEND(readability-make-member-function-const)

  virtual ~O3_CPU() noexcept = default;

  explicit O3_CPU(champsim::modules::ModuleBuilder builder)
      : core_module(builder.get_parameter<champsim::chrono::picoseconds>("clock_period")),
        DIB(builder.get_parameter<uint32_t>("dib_set"), builder.get_parameter<uint32_t>("dib_way"),
            {champsim::data::bits{champsim::lg2(builder.get_parameter<std::size_t>("dib_window"))}},
            {champsim::data::bits{champsim::lg2(builder.get_parameter<std::size_t>("dib_window"))}}),
        LQ(builder.get_parameter<uint32_t>("lq_size")), IFETCH_BUFFER_SIZE(builder.get_parameter<uint32_t>("ifetch_buffer_size")),
        DISPATCH_BUFFER_SIZE(builder.get_parameter<uint32_t>("dispatch_buffer_size")),
        DECODE_BUFFER_SIZE(builder.get_parameter<uint32_t>("decode_buffer_size")), REGISTER_FILE_SIZE(builder.get_parameter<uint32_t>("register_file_size")),
        ROB_SIZE(builder.get_parameter<uint32_t>("rob_size")), SQ_SIZE(builder.get_parameter<uint32_t>("sq_size")),
        DIB_HIT_BUFFER_SIZE(builder.get_parameter<uint32_t>("dib_hit_buffer_size")),
        FETCH_WIDTH(builder.get_parameter<champsim::bandwidth::maximum_type>("fetch_width")),
        DECODE_WIDTH(builder.get_parameter<champsim::bandwidth::maximum_type>("decode_width")),
        DISPATCH_WIDTH(builder.get_parameter<champsim::bandwidth::maximum_type>("dispatch_width")),
        SCHEDULER_SIZE(builder.get_parameter<champsim::bandwidth::maximum_type>("schedule_width")),
        EXEC_WIDTH(builder.get_parameter<champsim::bandwidth::maximum_type>("execute_width")),
        DIB_INORDER_WIDTH(builder.get_parameter<champsim::bandwidth::maximum_type>("dib_inorder_width")),
        LQ_WIDTH(builder.get_parameter<champsim::bandwidth::maximum_type>("lq_width")),
        SQ_WIDTH(builder.get_parameter<champsim::bandwidth::maximum_type>("sq_width")),
        RETIRE_WIDTH(builder.get_parameter<champsim::bandwidth::maximum_type>("retire_width")),
        BRANCH_MISPREDICT_PENALTY(builder.get_parameter<unsigned>("mispredict_penalty") * builder.get_parameter<champsim::chrono::picoseconds>("clock_period")),
        DISPATCH_LATENCY(builder.get_parameter<unsigned>("dispatch_latency") * builder.get_parameter<champsim::chrono::picoseconds>("clock_period")),
        DECODE_LATENCY(builder.get_parameter<unsigned>("decode_latency") * builder.get_parameter<champsim::chrono::picoseconds>("clock_period")),
        SCHEDULING_LATENCY(builder.get_parameter<unsigned>("schedule_latency") * builder.get_parameter<champsim::chrono::picoseconds>("clock_period")),
        EXEC_LATENCY(builder.get_parameter<unsigned>("execute_latency") * builder.get_parameter<champsim::chrono::picoseconds>("clock_period")),
        DIB_HIT_LATENCY(builder.get_parameter<unsigned>("dib_hit_latency") * builder.get_parameter<champsim::chrono::picoseconds>("clock_period")),
        L1I_BANDWIDTH(builder.get_parameter<champsim::bandwidth::maximum_type>("l1i_bandwidth")),
        L1D_BANDWIDTH(builder.get_parameter<champsim::bandwidth::maximum_type>("l1d_bandwidth")),
        IN_QUEUE_SIZE(2 * champsim::to_underlying(builder.get_parameter<champsim::bandwidth::maximum_type>("fetch_width"))),
        L1I_bus(builder.get_parameter<champsim::modules::channel_module*>("fetch_queues")),
        L1D_bus(builder.get_parameter<champsim::modules::channel_module*>("data_queues")), l1i(builder.get_parameter<champsim::modules::cache_module*>("l1i"))
  {
    // Construct branch predictor submodules
    for (const auto& sub : builder.get_submodules("branch_predictor"))
      branch_module_pimpl.push_back(champsim::modules::branch_predictor::create_instance(sub, static_cast<champsim::modules::core_module*>(this)));

    // Construct BTB submodules
    for (const auto& sub : builder.get_submodules("btb"))
      btb_module_pimpl.push_back(champsim::modules::btb::create_instance(sub, static_cast<champsim::modules::core_module*>(this)));

    // The core consumes instruction packets, so it attaches instruction_producer
    // children directly — the interface it is designed for.
    for (const auto& sub : builder.get_submodules("instruction_producer")) {
      instruction_producer_pimpl.push_back(
          champsim::modules::instruction_producer::create_instance(sub, static_cast<champsim::modules::packet_consumer*>(this)));
    }
  }
};

#ifdef SET_ASIDE_CHAMPSIM_MODULE
#undef SET_ASIDE_CHAMPSIM_MODULE
#define CHAMPSIM_MODULE
#endif

#endif
