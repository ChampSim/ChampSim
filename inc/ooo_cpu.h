#ifndef OOO_CPU_H
#define OOO_CPU_H

#include <array>
#include <deque>
#include <functional>
#include <limits>
#include <queue>
#include <vector>

#include "block.h"
#include "champsim.h"
#include "delay_queue.hpp"
#include "instruction.h"
#include "memory_class.h"
#include "operable.h"

class CacheBus : public MemoryRequestProducer
{
  uint32_t cpu;

public:
  champsim::circular_buffer<PACKET> PROCESSED;
  CacheBus(uint32_t cpu, std::size_t q_size, MemoryRequestConsumer* ll) : MemoryRequestProducer(ll), cpu(cpu), PROCESSED(q_size) {}
  bool issue_read(PACKET packet);
  bool issue_write(PACKET packet);
  void return_data(const PACKET &packet);
};

// cpu
class O3_CPU : public champsim::operable
{
public:
  uint32_t cpu = 0;

  // instruction
  uint64_t instr_unique_id = 0;
  uint64_t begin_sim_cycle = 0;
  uint64_t begin_sim_instr = 0;
  uint64_t last_sim_cycle = 0;
  uint64_t last_sim_instr = 0;
  uint64_t finish_sim_cycle = 0;
  uint64_t finish_sim_instr = 0;
  uint64_t instrs_to_read_this_cycle = 0;
  uint64_t instrs_to_fetch_this_cycle = 0;
  uint64_t next_print_instruction = STAT_PRINTING_PERIOD;
  uint64_t num_retired = 0;

  struct dib_entry_t {
    bool valid = false;
    unsigned lru = 999999;
    uint64_t address = 0;
  };

  // instruction buffer
  using dib_t = std::vector<dib_entry_t>;
  const std::size_t dib_set, dib_way, dib_window;
  dib_t DIB{dib_set * dib_way};

  // reorder buffer, load/store queue, register file
  champsim::circular_buffer<ooo_model_instr> IFETCH_BUFFER;
  champsim::circular_buffer<ooo_model_instr> DISPATCH_BUFFER;
  champsim::delay_queue<ooo_model_instr> DECODE_BUFFER;
  champsim::circular_buffer<ooo_model_instr> ROB;
  std::vector<LSQ_ENTRY> LQ;
  std::vector<LSQ_ENTRY> SQ;

  std::array<std::vector<champsim::circular_buffer<ooo_model_instr>::iterator>, std::numeric_limits<uint8_t>::max() + 1> reg_producers;

  // Constants
  const unsigned FETCH_WIDTH, DECODE_WIDTH, DISPATCH_WIDTH, SCHEDULER_SIZE, EXEC_WIDTH, LQ_WIDTH, SQ_WIDTH, RETIRE_WIDTH;
  const unsigned BRANCH_MISPREDICT_PENALTY, DISPATCH_LATENCY, SCHEDULING_LATENCY, EXEC_LATENCY;

  std::deque<uint64_t> STA; // this structure is required to properly handle store instructions

  std::queue<champsim::circular_buffer<ooo_model_instr>::iterator> ready_to_execute; // Ready-To-Execute

  // branch
  uint8_t fetch_stall = 0;
  uint64_t fetch_resume_cycle = 0;
  uint64_t num_branch = 0;
  uint64_t branch_mispredictions = 0;
  uint64_t total_rob_occupancy_at_branch_mispredict;

  uint64_t total_branch_types[8] = {};
  uint64_t branch_type_misses[8] = {};

  CacheBus ITLB_bus, DTLB_bus, L1I_bus, L1D_bus;

  void operate() override;

  void init_instruction(ooo_model_instr instr);
  void check_dib();
  void translate_fetch();
  void fetch_instruction();
  void promote_to_decode();
  void decode_instruction();
  void dispatch_instruction();
  void schedule_instruction();
  void execute_instruction();
  void schedule_memory_instruction();
  void execute_memory_instruction();
  void do_check_dib(ooo_model_instr& instr);
  void do_translate_fetch(champsim::circular_buffer<ooo_model_instr>::iterator begin, champsim::circular_buffer<ooo_model_instr>::iterator end);
  void do_fetch_instruction(champsim::circular_buffer<ooo_model_instr>::iterator begin, champsim::circular_buffer<ooo_model_instr>::iterator end);
  void do_dib_update(const ooo_model_instr& instr);
  void do_scheduling(champsim::circular_buffer<ooo_model_instr>::iterator rob_it);
  void do_execution(champsim::circular_buffer<ooo_model_instr>::iterator rob_it);
  void do_memory_scheduling(champsim::circular_buffer<ooo_model_instr>::iterator rob_it);
  void operate_lsq();
  void do_complete_execution(champsim::circular_buffer<ooo_model_instr>::iterator rob_it);
  void do_sq_forward_to_lq(LSQ_ENTRY& sq_entry, LSQ_ENTRY& lq_entry);

  void initialize_core();
  void execute_store(std::vector<LSQ_ENTRY>::iterator sq_it);
  bool execute_load(std::vector<LSQ_ENTRY>::iterator lq_it);
  bool do_translate_store(std::vector<LSQ_ENTRY>::iterator sq_it);
  bool do_translate_load(std::vector<LSQ_ENTRY>::iterator sq_it);
  void complete_inflight_instruction();
  void handle_memory_return();
  void retire_rob();

  void print_deadlock() override;

#include "ooo_cpu_modules.inc"

  const bpred_t bpred_type;
  const btb_t btb_type;

  O3_CPU(uint32_t cpu, double freq_scale, std::size_t dib_set, std::size_t dib_way, std::size_t dib_window, std::size_t ifetch_buffer_size,
         std::size_t decode_buffer_size, std::size_t dispatch_buffer_size, std::size_t rob_size, std::size_t lq_size, std::size_t sq_size, unsigned fetch_width,
         unsigned decode_width, unsigned dispatch_width, unsigned schedule_width, unsigned execute_width, unsigned lq_width, unsigned sq_width,
         unsigned retire_width, unsigned mispredict_penalty, unsigned decode_latency, unsigned dispatch_latency, unsigned schedule_latency,
         unsigned execute_latency, MemoryRequestConsumer* itlb, MemoryRequestConsumer* dtlb, MemoryRequestConsumer* l1i, MemoryRequestConsumer* l1d,
         bpred_t bpred_type, btb_t btb_type)
      : champsim::operable(freq_scale), cpu(cpu), dib_set(dib_set), dib_way(dib_way), dib_window(dib_window), IFETCH_BUFFER(ifetch_buffer_size),
        DISPATCH_BUFFER(dispatch_buffer_size), DECODE_BUFFER(decode_buffer_size, decode_latency), ROB(rob_size), LQ(lq_size), SQ(sq_size),
        FETCH_WIDTH(fetch_width), DECODE_WIDTH(decode_width), DISPATCH_WIDTH(dispatch_width), SCHEDULER_SIZE(schedule_width), EXEC_WIDTH(execute_width),
        LQ_WIDTH(lq_width), SQ_WIDTH(sq_width), RETIRE_WIDTH(retire_width), BRANCH_MISPREDICT_PENALTY(mispredict_penalty), DISPATCH_LATENCY(dispatch_latency),
        SCHEDULING_LATENCY(schedule_latency), EXEC_LATENCY(execute_latency), ITLB_bus(cpu, rob_size, itlb), DTLB_bus(cpu, rob_size, dtlb),
        L1I_bus(cpu, rob_size, l1i), L1D_bus(cpu, rob_size, l1d), bpred_type(bpred_type), btb_type(btb_type)
  {
  }
};

#endif
