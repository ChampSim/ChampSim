#ifndef REORDER_BUFFER_H
#define REORDER_BUFFER_H

#include <deque>
#include <limits>

#include "instruction.h"
#include "cache_bus.h"

namespace champsim
{
struct LSQ_ENTRY {
  uint64_t instr_id = 0;
  uint64_t virtual_address = 0;
  uint64_t ip = 0;
  uint64_t event_cycle = 0;

  std::array<uint8_t, 2> asid = {std::numeric_limits<uint8_t>::max(), std::numeric_limits<uint8_t>::max()};
  bool fetch_issued = false;

  uint64_t producer_id = std::numeric_limits<uint64_t>::max();
  std::vector<std::reference_wrapper<std::optional<LSQ_ENTRY>>> lq_depend_on_me{};

  LSQ_ENTRY(uint64_t id, uint64_t addr, uint64_t ip, std::array<uint8_t, 2> asid);
};

struct reorder_buffer
{
  using value_type = ooo_model_instr;
  using lq_value_type = std::optional<LSQ_ENTRY>;
  std::array<std::vector<std::reference_wrapper<value_type>>, std::numeric_limits<uint8_t>::max() + 1> reg_producers;

  bool warmup = true;
  std::size_t ROB_SIZE, SQ_SIZE;
  long SCHEDULER_SIZE, EXEC_WIDTH, RETIRE_WIDTH;
  long LQ_WIDTH, SQ_WIDTH, L1D_BANDWIDTH;
  uint64_t BRANCH_MISPREDICT_PENALTY, SCHEDULING_LATENCY, EXEC_LATENCY;

  uint64_t current_cycle = 0;
  uint64_t num_retired = 0;
  uint64_t stall_resume_cycle = 0;

  std::deque<value_type> ROB;
  std::deque<uint64_t> ROB_instr_ids;
  std::vector<lq_value_type> LQ;
  std::deque<LSQ_ENTRY> SQ;

  std::vector<uint64_t> ready_to_execute;

  champsim::CacheBus L1D_bus;

  void operate_sq();
  void operate_lq();
  void schedule_instruction();
  void execute_instruction();
  void complete_inflight_instruction();
  void retire_rob();
  void handle_memory_return();

  bool is_ready_to_execute(const value_type& instr) const;
  bool is_ready_to_complete(const value_type& instr) const;

  void do_scheduling(value_type& instr);
  void do_execution(value_type& rob_it);
  void do_complete_execution(value_type& instr);
  void do_memory_scheduling(value_type& instr);
  void do_forwarding_for_lq(lq_value_type& q_entry);

  void do_finish_store(const LSQ_ENTRY& sq_entry);
  bool do_complete_store(const LSQ_ENTRY& sq_entry);
  bool execute_load(const LSQ_ENTRY& lq_entry);

  std::deque<value_type>::iterator find_in_rob(uint64_t id);
  void finish(const LSQ_ENTRY& entry);

  explicit reorder_buffer(uint32_t cpu, std::size_t size, std::size_t lq_size, std::size_t sq_size, long sched_width, long exec_width, long lq_width, long sq_width,
      long l1d_bw, long retire_width, uint64_t mispredict_lat, uint64_t sched_lat, uint64_t exec_lat, champsim::channel* data_queues);

  void operate();

  std::size_t occupancy() const;
  std::size_t size() const;
  bool empty() const;
  bool full() const;
  bool would_accept(const value_type& inst) const;
  bool is_deadlocked() const;

  std::size_t lq_occupancy() const;
  std::size_t lq_size() const;
  std::size_t sq_occupancy() const;
  std::size_t sq_size() const;

  void push_back(const value_type& v);
  void push_back(value_type&& v);

  auto retired_count() const { return num_retired; }
  void set_stall() { stall_resume_cycle = std::numeric_limits<uint64_t>::max(); }
  auto get_stall_resume_cycle() const { return stall_resume_cycle; }

  void print_deadlock() const;
};
}

#endif
