#ifndef OOO_CPU_H
#define OOO_CPU_H

#include <array>
#include <bitset>
#include <deque>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <vector>

#include "champsim.h"
#include "champsim_constants.h"
#include "instruction.h"
#include "memory_class.h"
#include "operable.h"
#include "util.h"

enum STATUS { INFLIGHT = 1, COMPLETED = 2 };

class CacheBus : public MemoryRequestProducer
{
  uint32_t cpu;

public:
  std::deque<PACKET> PROCESSED;
  CacheBus(uint32_t cpu, MemoryRequestConsumer* ll) : MemoryRequestProducer(ll), cpu(cpu) {}
  bool issue_read(PACKET packet);
  bool issue_write(PACKET packet);
  void return_data(const PACKET& packet);
};

struct branch_stats {
  uint64_t total_rob_occupancy_at_branch_mispredict = 0;

  std::array<uint64_t, 8> total_branch_types = {};
  std::array<uint64_t, 8> branch_type_misses = {};
};

struct LSQ_ENTRY {
  uint64_t instr_id = 0;
  uint64_t virtual_address = 0;
  uint64_t ip = 0;
  uint64_t event_cycle = 0;

  ooo_model_instr& rob_entry;

  uint16_t asid = std::numeric_limits<uint16_t>::max();
  bool fetch_issued = false;

  uint64_t producer_id = std::numeric_limits<uint64_t>::max();
  std::vector<std::reference_wrapper<std::optional<LSQ_ENTRY>>> lq_depend_on_me;
};

// cpu
class O3_CPU : public champsim::operable
{
public:
  uint32_t cpu = 0;

  // cycle
  uint64_t begin_phase_cycle = 0;
  uint64_t begin_phase_instr = 0;
  uint64_t finish_phase_cycle = 0;
  uint64_t finish_phase_instr = 0;
  uint64_t last_heartbeat_cycle = 0;
  uint64_t last_heartbeat_instr = 0;
  uint64_t next_print_instruction = STAT_PRINTING_PERIOD;

  // instruction
  uint64_t instr_unique_id = 0;
  uint64_t instrs_to_read_this_cycle = 0;
  uint64_t instrs_to_fetch_this_cycle = 0;
  uint64_t num_retired = 0;

  bool show_heartbeat = true;

  using stats_type = branch_stats;

  std::vector<stats_type> roi_stats, sim_stats;

  // instruction buffer
  champsim::simple_lru_table<bool> DIB; //<bool> used here as placeholder. Data is not actually used.

  // reorder buffer, load/store queue, register file
  std::deque<ooo_model_instr> IFETCH_BUFFER;
  std::deque<ooo_model_instr> DISPATCH_BUFFER;
  std::deque<ooo_model_instr> DECODE_BUFFER;
  std::deque<ooo_model_instr> ROB;

  std::vector<std::optional<LSQ_ENTRY>> LQ;
  std::deque<LSQ_ENTRY> SQ;

  std::array<std::vector<std::reference_wrapper<ooo_model_instr>>, std::numeric_limits<uint8_t>::max() + 1> reg_producers;

  // Constants
  const std::size_t IFETCH_BUFFER_SIZE, DISPATCH_BUFFER_SIZE, DECODE_BUFFER_SIZE, ROB_SIZE, SQ_SIZE;
  const unsigned FETCH_WIDTH, DECODE_WIDTH, DISPATCH_WIDTH, SCHEDULER_SIZE, EXEC_WIDTH, LQ_WIDTH, SQ_WIDTH, RETIRE_WIDTH;
  const unsigned BRANCH_MISPREDICT_PENALTY, DISPATCH_LATENCY, DECODE_LATENCY, SCHEDULING_LATENCY, EXEC_LATENCY;
  const unsigned L1I_BANDWIDTH, L1D_BANDWIDTH;

  // branch
  uint8_t fetch_stall = 0;
  uint64_t fetch_resume_cycle = 0;

  const std::size_t IN_QUEUE_SIZE = 2 * FETCH_WIDTH;
  std::deque<ooo_model_instr> input_queue;

  CacheBus L1I_bus, L1D_bus;

  void initialize() override;
  void operate() override;
  void begin_phase() override;
  void end_phase(unsigned cpu) override;
  void print_roi_stats() override;
  void print_phase_stats() override;

  void initialize_instruction();
  void check_dib();
  void translate_fetch();
  void fetch_instruction();
  void promote_to_decode();
  void decode_instruction();
  void dispatch_instruction();
  void schedule_instruction();
  void execute_instruction();
  void schedule_memory_instruction();
  void operate_lsq();
  void complete_inflight_instruction();
  void handle_memory_return();
  void retire_rob();

  void do_init_instruction(ooo_model_instr& instr);
  void do_check_dib(ooo_model_instr& instr);
  bool do_fetch_instruction(std::deque<ooo_model_instr>::iterator begin, std::deque<ooo_model_instr>::iterator end);
  void do_dib_update(const ooo_model_instr& instr);
  void do_scheduling(ooo_model_instr& instr);
  void do_execution(ooo_model_instr& rob_it);
  void do_memory_scheduling(ooo_model_instr& instr);
  void do_complete_execution(ooo_model_instr& instr);
  void do_sq_forward_to_lq(LSQ_ENTRY& sq_entry, LSQ_ENTRY& lq_entry);

  void do_finish_store(LSQ_ENTRY& sq_entry);
  bool do_complete_store(const LSQ_ENTRY& sq_entry);
  bool execute_load(const LSQ_ENTRY& lq_entry);

  uint64_t roi_instr() const { return finish_phase_instr - begin_phase_instr; }
  uint64_t roi_cycle() const { return finish_phase_cycle - begin_phase_cycle; }
  uint64_t sim_instr() const { return num_retired - begin_phase_instr; }
  uint64_t sim_cycle() const { return current_cycle - begin_phase_cycle; }

  void print_deadlock() override;

#include "ooo_cpu_modules.inc"

  const std::bitset<NUM_BRANCH_MODULES> bpred_type;
  const std::bitset<NUM_BTB_MODULES> btb_type;

  O3_CPU(uint32_t cpu, double freq_scale, champsim::simple_lru_table<bool>&& dib, std::size_t ifetch_buffer_size, std::size_t decode_buffer_size,
         std::size_t dispatch_buffer_size, std::size_t rob_size, std::size_t lq_size, std::size_t sq_size, unsigned fetch_width, unsigned decode_width,
         unsigned dispatch_width, unsigned schedule_width, unsigned execute_width, unsigned lq_width, unsigned sq_width, unsigned retire_width,
         unsigned mispredict_penalty, unsigned decode_latency, unsigned dispatch_latency, unsigned schedule_latency, unsigned execute_latency,
         MemoryRequestConsumer* l1i, unsigned l1i_bw, MemoryRequestConsumer* l1d, unsigned l1d_bw, std::bitset<NUM_BRANCH_MODULES> bpred_type,
         std::bitset<NUM_BTB_MODULES> btb_type)
      : champsim::operable(freq_scale), cpu(cpu), DIB{std::move(dib)}, LQ(lq_size), IFETCH_BUFFER_SIZE(ifetch_buffer_size),
        DISPATCH_BUFFER_SIZE(dispatch_buffer_size), DECODE_BUFFER_SIZE(decode_buffer_size), ROB_SIZE(rob_size), SQ_SIZE(sq_size), FETCH_WIDTH(fetch_width),
        DECODE_WIDTH(decode_width), DISPATCH_WIDTH(dispatch_width), SCHEDULER_SIZE(schedule_width), EXEC_WIDTH(execute_width), LQ_WIDTH(lq_width),
        SQ_WIDTH(sq_width), RETIRE_WIDTH(retire_width), BRANCH_MISPREDICT_PENALTY(mispredict_penalty), DISPATCH_LATENCY(dispatch_latency),
        DECODE_LATENCY(decode_latency), SCHEDULING_LATENCY(schedule_latency), EXEC_LATENCY(execute_latency), L1I_BANDWIDTH(l1i_bw), L1D_BANDWIDTH(l1d_bw),
        L1I_bus(cpu, l1i), L1D_bus(cpu, l1d), bpred_type(bpred_type), btb_type(btb_type)
  {
  }
};

#endif
