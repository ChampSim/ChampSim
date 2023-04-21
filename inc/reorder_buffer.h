#ifndef REORDER_BUFFER_H
#define REORDER_BUFFER_H

#include <deque>
#include <iostream>
#include <limits>

#include "champsim.h"
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
  template <typename It>
  void finish(It begin, It end) const;
};

struct reorder_buffer
{
  using value_type = ooo_model_instr;
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
  std::vector<std::optional<LSQ_ENTRY>> LQ;
  std::deque<LSQ_ENTRY> SQ;

  champsim::CacheBus L1D_bus;

  void operate_lsq();
  void schedule_instruction();
  void execute_instruction();
  void complete_inflight_instruction();
  void retire_rob();
  void handle_memory_return();

  void do_scheduling(value_type& instr);
  void do_execution(value_type& rob_it);
  void do_complete_execution(value_type& instr);
  void do_memory_scheduling(ooo_model_instr& instr);

  void do_finish_store(const LSQ_ENTRY& sq_entry);
  bool do_complete_store(const LSQ_ENTRY& sq_entry);
  bool execute_load(const LSQ_ENTRY& lq_entry);

  explicit reorder_buffer(uint32_t cpu, std::size_t size, std::size_t lq_size, std::size_t sq_size, long sched_width, long exec_width, long lq_width, long sq_width,
      long l1d_bw, long retire_width, uint64_t mispredict_lat, uint64_t sched_lat, uint64_t exec_lat, champsim::channel* data_queues)
    : ROB_SIZE(size), SQ_SIZE(sq_size), SCHEDULER_SIZE(sched_width), EXEC_WIDTH(exec_width), RETIRE_WIDTH(retire_width),
      LQ_WIDTH(lq_width), SQ_WIDTH(sq_width), L1D_BANDWIDTH(l1d_bw),
      BRANCH_MISPREDICT_PENALTY(mispredict_lat), SCHEDULING_LATENCY(sched_lat), EXEC_LATENCY(exec_lat), LQ(lq_size), L1D_bus(cpu, data_queues)
  {}

  void operate()
  {
    retire_rob();                    // retire
    complete_inflight_instruction(); // finalize execution
    execute_instruction();           // execute instructions
    schedule_instruction();          // schedule instructions
    handle_memory_return();
    ++current_cycle;
  }

  std::size_t occupancy() const { return std::size(ROB); }
  std::size_t size() const { return ROB_SIZE; }
  bool empty() const { return std::empty(ROB); }
  bool full() const { return occupancy() == size(); }

  bool would_accept(const value_type& inst) const {
    return !full()
      && ((std::size_t)std::count_if(std::begin(LQ), std::end(LQ), std::not_fn(is_valid<decltype(LQ)::value_type>{})) >= std::size(inst.source_memory))
      && ((std::size(inst.destination_memory) + std::size(SQ)) <= SQ_SIZE);
  }

  bool is_deadlocked() const { return !empty() && (ROB.front().event_cycle + DEADLOCK_CYCLE) <= current_cycle; }

  auto begin() { return ROB.begin(); }
  auto end() { return ROB.end(); }

  void push_back(value_type& v) {
    ROB.push_back(v);
    do_memory_scheduling(ROB.back());
  }
  void push_back(value_type&& v) {
    ROB.push_back(std::move(v));
    do_memory_scheduling(ROB.back());
  }

  uint64_t retired_count() const { return num_retired; }
  void set_stall() { stall_resume_cycle = std::numeric_limits<uint64_t>::max(); }
  auto get_stall_resume_cycle() { return stall_resume_cycle; }

  void print_deadlock() const
  {
    if (!std::empty(ROB)) {
      std::cout << "ROB head";
      std::cout << " instr_id: " << ROB.front().instr_id;
      std::cout << " fetched: " << +ROB.front().fetched;
      std::cout << " scheduled: " << +ROB.front().scheduled;
      std::cout << " executed: " << +ROB.front().executed;
      std::cout << " num_reg_dependent: " << +ROB.front().num_reg_dependent;
      std::cout << " num_mem_ops: " << ROB.front().num_mem_ops() - ROB.front().completed_mem_ops;
      std::cout << " event: " << ROB.front().event_cycle;
      std::cout << std::endl;
    } else {
      std::cout << "ROB empty" << std::endl;
    }

    // print LQ entry
    std::cout << "Load Queue Entry" << std::endl;
    for (auto lq_it = std::begin(LQ); lq_it != std::end(LQ); ++lq_it) {
      if (lq_it->has_value()) {
        std::cout << "[LQ] entry: " << std::distance(std::begin(LQ), lq_it) << " instr_id: " << (*lq_it)->instr_id << " address: " << std::hex
          << (*lq_it)->virtual_address << std::dec << " fetched_issued: " << std::boolalpha << (*lq_it)->fetch_issued << std::noboolalpha
          << " event_cycle: " << (*lq_it)->event_cycle;
        if ((*lq_it)->producer_id != std::numeric_limits<uint64_t>::max())
          std::cout << " waits on " << (*lq_it)->producer_id;
        std::cout << std::endl;
      }
    }

    // print SQ entry
    std::cout << std::endl << "Store Queue Entry" << std::endl;
    for (auto sq_it = std::begin(SQ); sq_it != std::end(SQ); ++sq_it) {
      std::cout << "[SQ] entry: " << std::distance(std::begin(SQ), sq_it) << " instr_id: " << sq_it->instr_id << " address: " << std::hex
        << sq_it->virtual_address << std::dec << " fetched: " << std::boolalpha << sq_it->fetch_issued << std::noboolalpha
        << " event_cycle: " << sq_it->event_cycle << " LQ waiting: ";
      for (std::optional<LSQ_ENTRY>& lq_entry : sq_it->lq_depend_on_me)
        std::cout << lq_entry->instr_id << " ";
      std::cout << std::endl;
    }
  }
};
}

#endif
