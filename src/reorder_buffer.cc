#include "reorder_buffer.h"

#include <iostream>

#include "champsim.h"
#include "champsim_constants.h"

champsim::reorder_buffer::reorder_buffer(uint32_t cpu, std::size_t size, std::size_t lq_size, std::size_t sq_size, long sched_width, long exec_width, long lq_width, long sq_width,
    long l1d_bw, long retire_width, uint64_t mispredict_lat, uint64_t sched_lat, uint64_t exec_lat, champsim::channel* data_queues)
: ROB_SIZE(size), SQ_SIZE(sq_size), SCHEDULER_SIZE(sched_width), EXEC_WIDTH(exec_width), RETIRE_WIDTH(retire_width),
  LQ_WIDTH(lq_width), SQ_WIDTH(sq_width), L1D_BANDWIDTH(l1d_bw),
  BRANCH_MISPREDICT_PENALTY(mispredict_lat), SCHEDULING_LATENCY(sched_lat), EXEC_LATENCY(exec_lat), LQ(lq_size), L1D_bus(cpu, data_queues)
{}

champsim::LSQ_ENTRY::LSQ_ENTRY(uint64_t id, uint64_t addr, uint64_t local_ip, std::array<uint8_t, 2> local_asid)
    : instr_id(id), virtual_address(addr), ip(local_ip), asid(local_asid)
{
}

void champsim::reorder_buffer::operate()
{
  retire_rob();                    // retire
  complete_inflight_instruction(); // finalize execution
  execute_instruction();           // execute instructions
  schedule_instruction();          // schedule instructions
  handle_memory_return();
  operate_sq();
  operate_lq();
  ++current_cycle;
}

bool champsim::reorder_buffer::would_accept(const value_type& inst) const
{
  return !full()
    && ((lq_size() - lq_occupancy()) >= std::size(inst.source_memory))
    && ((sq_size() - sq_occupancy()) >= std::size(inst.destination_memory));
}

void champsim::reorder_buffer::push_back(const value_type& v) {
  ROB.push_back(v);
  ROB_instr_ids.push_back(ROB.back().instr_id);
  do_memory_scheduling(ROB.back());
}

void champsim::reorder_buffer::push_back(value_type&& v) {
  ROB.push_back(std::move(v));
  ROB_instr_ids.push_back(ROB.back().instr_id);
  do_memory_scheduling(ROB.back());
}

void champsim::reorder_buffer::schedule_instruction()
{
  auto search_bw = SCHEDULER_SIZE;
  for (auto rob_it = std::begin(ROB); rob_it != std::end(ROB) && search_bw > 0; ++rob_it) {
    if (!rob_it->scheduled)
      do_scheduling(*rob_it);

    if (!rob_it->executed)
      --search_bw;
  }
}

void champsim::reorder_buffer::do_scheduling(value_type& instr)
{
  // Mark register dependencies
  for (auto src_reg : instr.source_registers) {
    if (!std::empty(reg_producers[src_reg])) {
      value_type& prior = reg_producers[src_reg].back();
      if (prior.registers_instrs_depend_on_me.empty() || prior.registers_instrs_depend_on_me.back() != instr.instr_id) {
        prior.registers_instrs_depend_on_me.push_back(instr.instr_id);
        instr.num_reg_dependent++;
      }
    }
  }

  for (auto dreg : instr.destination_registers) {
    auto begin = std::begin(reg_producers[dreg]);
    auto end = std::end(reg_producers[dreg]);
    auto ins = std::lower_bound(begin, end, instr, [](const value_type& lhs, const value_type& rhs) { return lhs.instr_id < rhs.instr_id; });
    reg_producers[dreg].insert(ins, std::ref(instr));
  }

  instr.scheduled = true;
  instr.event_cycle = current_cycle + (warmup ? 0 : SCHEDULING_LATENCY);
}

bool champsim::reorder_buffer::is_ready_to_execute(const value_type& instr) const
{
  return instr.event_cycle <= current_cycle && instr.scheduled && !instr.executed && instr.num_reg_dependent == 0;
}

void champsim::reorder_buffer::execute_instruction()
{
  auto exec_bw = EXEC_WIDTH;
  for (auto rob_it = std::begin(ROB); rob_it != std::end(ROB) && exec_bw > 0; ++rob_it) {
    if (is_ready_to_execute(*rob_it)) {
      do_execution(*rob_it);
      --exec_bw;
    }
  }
}

void champsim::reorder_buffer::do_execution(value_type& rob_entry)
{
  rob_entry.executed = true;
  rob_entry.event_cycle = current_cycle + (warmup ? 0 : EXEC_LATENCY);

  // Mark LQ entries as ready to translate
  for (auto& lq_entry : LQ)
    if (lq_entry.has_value() && lq_entry->instr_id == rob_entry.instr_id)
      lq_entry->event_cycle = current_cycle + (warmup ? 0 : EXEC_LATENCY);

  // Mark SQ entries as ready to translate
  for (auto& sq_entry : SQ)
    if (sq_entry.instr_id == rob_entry.instr_id)
      sq_entry.event_cycle = current_cycle + (warmup ? 0 : EXEC_LATENCY);

  if constexpr (champsim::debug_print) {
    std::cout << "[ROB] " << __func__ << " instr_id: " << rob_entry.instr_id << " event_cycle: " << rob_entry.event_cycle << std::endl;
  }
}

void champsim::reorder_buffer::do_memory_scheduling(value_type& instr)
{
  // load
  for (auto& smem : instr.source_memory) {
    auto q_entry = std::find_if_not(std::begin(LQ), std::end(LQ), [](const auto& x) { return x.has_value(); });
    assert(q_entry != std::end(LQ));
    q_entry->emplace(instr.instr_id, smem, instr.ip, instr.asid); // add it to the load queue
    do_forwarding_for_lq(*q_entry);
  }

  // store
  for (auto& dmem : instr.destination_memory)
    SQ.emplace_back(instr.instr_id, dmem, instr.ip, instr.asid); // add it to the store queue

  if constexpr (champsim::debug_print) {
    std::cout << "[DISPATCH] " << __func__ << " instr_id: " << instr.instr_id << " loads: " << std::size(instr.source_memory)
              << " stores: " << std::size(instr.destination_memory) << std::endl;
  }
}

void champsim::reorder_buffer::do_forwarding_for_lq(lq_value_type& q_entry)
{
  // Check for forwarding
  auto sq_it = std::max_element(std::begin(SQ), std::end(SQ), [smem=q_entry->virtual_address](const auto& lhs, const auto& rhs) {
    return lhs.virtual_address != smem || (rhs.virtual_address == smem && lhs.instr_id < rhs.instr_id);
  });
  if (sq_it != std::end(SQ) && sq_it->virtual_address == q_entry->virtual_address) {
    if (sq_it->fetch_issued) { // Store already executed
      finish(*q_entry);
      q_entry.reset();

      if constexpr (champsim::debug_print)
        std::cout << "[DISPATCH] " << __func__ << " instr_id: " << q_entry->instr_id << " forwards from " << sq_it->instr_id << std::endl;
    } else {
      assert(sq_it->instr_id < q_entry->instr_id);   // The found SQ entry is a prior store
      sq_it->lq_depend_on_me.push_back(q_entry); // Forward the load when the store finishes
      q_entry->producer_id = sq_it->instr_id;  // The load waits on the store to finish

      if constexpr (champsim::debug_print)
        std::cout << "[DISPATCH] " << __func__ << " instr_id: " << q_entry->instr_id << " waits on " << sq_it->instr_id << std::endl;
    }
  }
}

void champsim::reorder_buffer::operate_sq()
{
  auto store_bw = SQ_WIDTH;

  const auto complete_id = std::empty(ROB_instr_ids) ? std::numeric_limits<uint64_t>::max() : ROB_instr_ids.front();
  auto do_complete = [cycle = current_cycle, complete_id, this](const auto& x) {
    return x.instr_id < complete_id && x.event_cycle <= cycle && this->do_complete_store(x);
  };

  auto [complete_begin, complete_end] = champsim::get_span_p(std::cbegin(SQ), std::cend(SQ), store_bw, do_complete);
  store_bw -= std::distance(complete_begin, complete_end);
  SQ.erase(complete_begin, complete_end);

  auto unfetched_begin = std::partition_point(std::begin(SQ), std::end(SQ), [](const auto& x) { return x.fetch_issued; });
  auto [fetch_begin, fetch_end] = champsim::get_span_p(unfetched_begin, std::end(SQ), store_bw,
                                                       [cycle = current_cycle](const auto& x) { return !x.fetch_issued && x.event_cycle <= cycle; });
  store_bw -= std::distance(fetch_begin, fetch_end);
  std::for_each(fetch_begin, fetch_end, [cycle = current_cycle, this](auto& sq_entry) {
    this->do_finish_store(sq_entry);
    sq_entry.fetch_issued = true;
    sq_entry.event_cycle = cycle;
  });
}

void champsim::reorder_buffer::operate_lq()
{
  auto load_bw = LQ_WIDTH;

  for (auto& lq_entry : LQ) {
    if (load_bw > 0 && lq_entry.has_value() && lq_entry->producer_id == std::numeric_limits<uint64_t>::max() && !lq_entry->fetch_issued
        && lq_entry->event_cycle < current_cycle) {
      auto success = execute_load(*lq_entry);
      if (success) {
        --load_bw;
        lq_entry->fetch_issued = true;
      }
    }
  }
}

void champsim::reorder_buffer::do_finish_store(const LSQ_ENTRY& sq_entry)
{
  finish(sq_entry);

  // Release dependent loads
  for (std::optional<LSQ_ENTRY>& dependent : sq_entry.lq_depend_on_me) {
    assert(dependent.has_value()); // LQ entry is still allocated
    assert(dependent->producer_id == sq_entry.instr_id);

    finish(*dependent);
    dependent.reset();
  }
}

bool champsim::reorder_buffer::do_complete_store(const LSQ_ENTRY& sq_entry)
{
  CacheBus::request_type data_packet;
  data_packet.v_address = sq_entry.virtual_address;
  data_packet.instr_id = sq_entry.instr_id;
  data_packet.ip = sq_entry.ip;

  if constexpr (champsim::debug_print) {
    std::cout << "[SQ] " << __func__ << " instr_id: " << sq_entry.instr_id << std::endl;
  }

  return L1D_bus.issue_write(data_packet);
}

bool champsim::reorder_buffer::execute_load(const LSQ_ENTRY& lq_entry)
{
  CacheBus::request_type data_packet;
  data_packet.v_address = lq_entry.virtual_address;
  data_packet.instr_id = lq_entry.instr_id;
  data_packet.ip = lq_entry.ip;

  if constexpr (champsim::debug_print) {
    std::cout << "[LQ] " << __func__ << " instr_id: " << lq_entry.instr_id << std::endl;
  }

  return L1D_bus.issue_read(data_packet);
}

void champsim::reorder_buffer::do_complete_execution(value_type& instr)
{
  for (auto dreg : instr.destination_registers) {
    auto begin = std::begin(reg_producers[dreg]);
    auto end = std::end(reg_producers[dreg]);
    auto elem = std::find_if(begin, end, [id = instr.instr_id](value_type& x) { return x.instr_id == id; });
    assert(elem != end);
    reg_producers[dreg].erase(elem);
  }

  instr.completed = true;

  for (auto dependent_id : instr.registers_instrs_depend_on_me) {
    auto dep_it = find_in_rob(dependent_id);
    dep_it->num_reg_dependent--;
    assert(dep_it->num_reg_dependent >= 0);
  }

  if (instr.branch_mispredicted)
    stall_resume_cycle = current_cycle + BRANCH_MISPREDICT_PENALTY;
}

bool champsim::reorder_buffer::is_ready_to_complete(const value_type& instr) const
{
  return instr.event_cycle <= current_cycle && instr.executed && !instr.completed && instr.completed_mem_ops == instr.num_mem_ops();
}

void champsim::reorder_buffer::complete_inflight_instruction()
{
  // update ROB entries with completed executions
  auto complete_bw = EXEC_WIDTH;
  for (auto rob_it = std::begin(ROB); rob_it != std::end(ROB) && complete_bw > 0; ++rob_it) {
    if (is_ready_to_complete(*rob_it)) {
      do_complete_execution(*rob_it);
      --complete_bw;
    }
  }
}

void champsim::reorder_buffer::handle_memory_return()
{
  auto l1d_it = std::begin(L1D_bus.lower_level->returned);
  for (auto l1d_bw = L1D_BANDWIDTH; l1d_bw > 0 && l1d_it != std::end(L1D_bus.lower_level->returned); --l1d_bw, ++l1d_it) {
    for (auto& lq_entry : LQ) {
      if (lq_entry.has_value() && lq_entry->fetch_issued && lq_entry->virtual_address >> LOG2_BLOCK_SIZE == l1d_it->v_address >> LOG2_BLOCK_SIZE) {
        finish(*lq_entry);
        lq_entry.reset();
      }
    }
  }
  L1D_bus.lower_level->returned.erase(std::begin(L1D_bus.lower_level->returned), l1d_it);
}

void champsim::reorder_buffer::retire_rob()
{
  auto [retire_begin, retire_end] = champsim::get_span_p(std::cbegin(ROB), std::cend(ROB), RETIRE_WIDTH, [](const auto& x) { return x.completed; });
  if constexpr (champsim::debug_print) {
    std::for_each(retire_begin, retire_end, [](const auto& x) { std::cout << "[ROB] retire_rob instr_id: " << x.instr_id << " is retired" << std::endl; });
  }
  auto retire_count = std::distance(retire_begin, retire_end);
  num_retired += retire_count;
  ROB.erase(retire_begin, retire_end);
  ROB_instr_ids.erase(std::begin(ROB_instr_ids), std::next(std::begin(ROB_instr_ids), retire_count));
}

auto champsim::reorder_buffer::find_in_rob(uint64_t id) -> std::deque<value_type>::iterator
{
  auto id_it = std::find(std::begin(ROB_instr_ids), std::end(ROB_instr_ids), id);
  return std::next(std::begin(ROB), std::distance(std::begin(ROB_instr_ids), id_it));
}

std::size_t champsim::reorder_buffer::occupancy() const { return std::size(ROB); }
std::size_t champsim::reorder_buffer::size() const { return ROB_SIZE; }
bool champsim::reorder_buffer::empty() const { return std::empty(ROB); }
bool champsim::reorder_buffer::full() const { return occupancy() == size(); }

std::size_t champsim::reorder_buffer::lq_occupancy() const { return std::count_if(std::begin(LQ), std::end(LQ), [](const auto& x) { return x.has_value(); }); }
std::size_t champsim::reorder_buffer::lq_size() const { return std::size(LQ); }
std::size_t champsim::reorder_buffer::sq_occupancy() const { return std::size(SQ); }
std::size_t champsim::reorder_buffer::sq_size() const { return SQ_SIZE; }

// LCOV_EXCL_START
bool champsim::reorder_buffer::is_deadlocked() const
{
  return !empty() && (ROB.front().event_cycle + DEADLOCK_CYCLE) <= current_cycle;
}

void champsim::reorder_buffer::print_deadlock() const
{
  if (!std::empty(ROB)) {
    std::cout << "ROB head";
    std::cout << " instr_id: " << ROB.front().instr_id;
    std::cout << " fetched: " << +ROB.front().fetched;
    std::cout << " scheduled: " << std::boolalpha << ROB.front().scheduled << std::noboolalpha;
    std::cout << " executed: " << std::boolalpha << ROB.front().executed << std::noboolalpha;
    std::cout << " completed: " << std::boolalpha << ROB.front().completed << std::noboolalpha;
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
// LCOV_EXCL_STOP

void champsim::reorder_buffer::finish(const LSQ_ENTRY& entry)
{
  auto rob_entry = find_in_rob(entry.instr_id);
  assert(rob_entry != std::end(ROB));
  assert(rob_entry->instr_id == entry.instr_id);

  ++rob_entry->completed_mem_ops;
  assert(rob_entry->completed_mem_ops <= rob_entry->num_mem_ops());

  if constexpr (champsim::debug_print) {
    std::cout << "[LSQ] " << __func__ << " instr_id: " << entry.instr_id << std::hex;
    std::cout << " full_address: " << entry.virtual_address << std::dec << " remain_mem_ops: " << rob_entry->num_mem_ops() - rob_entry->completed_mem_ops;
    std::cout << " event_cycle: " << entry.event_cycle << std::endl;
  }
}

