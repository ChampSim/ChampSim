#ifndef OOO_CPU_H
#define OOO_CPU_H

#include <array>
#include <deque>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <vector>

#include "champsim_constants.h"
#include "delay_queue.hpp"
#include "instruction.h"
#include "memory_class.h"
#include "operable.h"

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

struct LSQ_ENTRY {
  uint64_t instr_id = 0;
  uint64_t virtual_address = 0;
  uint64_t ip = 0;
  uint64_t event_cycle = 0;

  ooo_model_instr& rob_entry;

  uint8_t asid[2] = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};
  bool translate_issued = false;
  bool fetch_issued = false;

  uint64_t physical_address = 0;
  uint64_t producer_id = std::numeric_limits<uint64_t>::max();
  std::vector<std::reference_wrapper<std::optional<LSQ_ENTRY>>> lq_depend_on_me;
};

// cpu
class O3_CPU : public champsim::operable
{
public:
  uint32_t cpu = 0;
  int trace_drained = 0;

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
  std::deque<ooo_model_instr> DISPATCH_BUFFER;
  champsim::delay_queue<ooo_model_instr> DECODE_BUFFER;
  std::deque<ooo_model_instr> ROB;

  std::vector<std::optional<LSQ_ENTRY>> LQ;
  std::deque<LSQ_ENTRY> SQ;

  std::array<std::vector<std::reference_wrapper<ooo_model_instr>>, std::numeric_limits<uint8_t>::max() + 1> reg_producers;

  // Constants
  const std::size_t DISPATCH_BUFFER_SIZE, ROB_SIZE, SQ_SIZE;
  const unsigned FETCH_WIDTH, DECODE_WIDTH, DISPATCH_WIDTH, SCHEDULER_SIZE, EXEC_WIDTH, LQ_WIDTH, SQ_WIDTH, RETIRE_WIDTH;
  const unsigned BRANCH_MISPREDICT_PENALTY, DISPATCH_LATENCY, SCHEDULING_LATENCY, EXEC_LATENCY;

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
  void do_scheduling(ooo_model_instr& instr);
  void do_execution(ooo_model_instr& rob_it);
  void do_memory_scheduling(ooo_model_instr& instr);
  void operate_lsq();
  void do_complete_execution(ooo_model_instr& instr);
  void do_sq_forward_to_lq(LSQ_ENTRY& sq_entry, LSQ_ENTRY& lq_entry);

  void initialize_core();
  void do_finish_store(LSQ_ENTRY& sq_entry);
  bool do_complete_store(const LSQ_ENTRY& sq_entry);
  bool execute_load(const LSQ_ENTRY& lq_entry);
  bool do_translate_store(const LSQ_ENTRY& sq_entry);
  bool do_translate_load(const LSQ_ENTRY& lq_entry);
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
        DECODE_BUFFER(decode_buffer_size, decode_latency), LQ(lq_size), DISPATCH_BUFFER_SIZE(dispatch_buffer_size), ROB_SIZE(rob_size), SQ_SIZE(sq_size),
        FETCH_WIDTH(fetch_width), DECODE_WIDTH(decode_width), DISPATCH_WIDTH(dispatch_width), SCHEDULER_SIZE(schedule_width), EXEC_WIDTH(execute_width),
        LQ_WIDTH(lq_width), SQ_WIDTH(sq_width), RETIRE_WIDTH(retire_width), BRANCH_MISPREDICT_PENALTY(mispredict_penalty), DISPATCH_LATENCY(dispatch_latency),
        SCHEDULING_LATENCY(schedule_latency), EXEC_LATENCY(execute_latency), ITLB_bus(cpu, itlb), DTLB_bus(cpu, dtlb), L1I_bus(cpu, l1i), L1D_bus(cpu, l1d),
        bpred_type(bpred_type), btb_type(btb_type)
  {
  }
};

#endif
