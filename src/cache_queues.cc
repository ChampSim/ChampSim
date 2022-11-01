#include "cache.h"
#include "champsim.h"
#include "instruction.h"
#include "util.h"

void CACHE::NonTranslatingQueues::operate() { check_collision(); }

void CACHE::TranslatingQueues::operate()
{
  NonTranslatingQueues::operate();
  issue_translation();
  detect_misses();
}

template <typename Iter, typename F>
bool do_collision_for(Iter begin, Iter end, PACKET& packet, unsigned shamt, F&& func)
{
  auto found = std::find_if(begin, end, eq_addr<PACKET>(packet.address, shamt));
  if (found != end) {
    func(packet, *found);
    return true;
  }

  return false;
}

template <typename Iter>
bool do_collision_for_merge(Iter begin, Iter end, PACKET& packet, unsigned shamt)
{
  return do_collision_for(begin, end, packet, shamt, [](PACKET& source, PACKET& destination) {
    auto instr_copy = std::move(destination.instr_depend_on_me);
    auto ret_copy = std::move(destination.to_return);

    std::set_union(std::begin(instr_copy), std::end(instr_copy), std::begin(source.instr_depend_on_me), std::end(source.instr_depend_on_me),
                   std::back_inserter(destination.instr_depend_on_me), [](ooo_model_instr& x, ooo_model_instr& y) { return x.instr_id < y.instr_id; });
    std::set_union(std::begin(ret_copy), std::end(ret_copy), std::begin(source.to_return), std::end(source.to_return),
                   std::back_inserter(destination.to_return));
  });
}

template <typename Iter>
bool do_collision_for_return(Iter begin, Iter end, PACKET& packet, unsigned shamt)
{
  return do_collision_for(begin, end, packet, shamt, [](PACKET& source, PACKET& destination) {
    source.data = destination.data;
    for (auto ret : source.to_return)
      ret->return_data(source);
  });
}

void CACHE::NonTranslatingQueues::check_collision()
{
  std::size_t write_shamt = match_offset_bits ? 0 : OFFSET_BITS;
  std::size_t read_shamt = OFFSET_BITS;

  // Check WQ for duplicates, merging if they are found
  for (auto wq_it = std::find_if(std::begin(WQ), std::end(WQ), std::not_fn(&PACKET::forward_checked)); wq_it != std::end(WQ);) {
    if (do_collision_for_merge(std::begin(WQ), wq_it, *wq_it, write_shamt)) {
      sim_stats.back().WQ_MERGED++;
      wq_it = WQ.erase(wq_it);
    } else {
      wq_it->forward_checked = true;
      ++wq_it;
    }
  }

  // Check RQ for forwarding from WQ (return if found), then for duplicates (merge if found)
  for (auto rq_it = std::find_if(std::begin(RQ), std::end(RQ), std::not_fn(&PACKET::forward_checked)); rq_it != std::end(RQ);) {
    if (do_collision_for_return(std::begin(WQ), std::end(WQ), *rq_it, write_shamt)) {
      sim_stats.back().WQ_FORWARD++;
      rq_it = RQ.erase(rq_it);
    } else if (do_collision_for_merge(std::begin(RQ), rq_it, *rq_it, read_shamt)) {
      sim_stats.back().RQ_MERGED++;
      rq_it = RQ.erase(rq_it);
    } else {
      rq_it->forward_checked = true;
      ++rq_it;
    }
  }

  // Check PQ for forwarding from WQ (return if found), then for duplicates (merge if found)
  for (auto pq_it = std::find_if(std::begin(PQ), std::end(PQ), std::not_fn(&PACKET::forward_checked)); pq_it != std::end(PQ);) {
    if (do_collision_for_return(std::begin(WQ), std::end(WQ), *pq_it, write_shamt)) {
      sim_stats.back().WQ_FORWARD++;
      pq_it = PQ.erase(pq_it);
    } else if (do_collision_for_merge(std::begin(PQ), pq_it, *pq_it, read_shamt)) {
      sim_stats.back().PQ_MERGED++;
      pq_it = PQ.erase(pq_it);
    } else {
      pq_it->forward_checked = true;
      ++pq_it;
    }
  }
}

void CACHE::TranslatingQueues::issue_translation()
{
  do_issue_translation(WQ);
  do_issue_translation(RQ);
  do_issue_translation(PQ);
}

template <typename R>
void CACHE::TranslatingQueues::do_issue_translation(R& queue)
{
  for (auto& q_entry : queue) {
    if (!q_entry.translate_issued && q_entry.address == q_entry.v_address) {
      auto fwd_pkt = q_entry;
      fwd_pkt.type = LOAD;
      fwd_pkt.to_return = {this};
      auto success = lower_level->add_rq(fwd_pkt);
      if (success) {
        if constexpr (champsim::debug_print) {
          std::cout << "[TRANSLATE] " << __func__ << " instr_id: " << q_entry.instr_id;
          std::cout << " address: " << std::hex << q_entry.address << " v_address: " << q_entry.v_address << std::dec;
          std::cout << " type: " << +q_entry.type << " occupancy: " << std::size(queue) << std::endl;
        }

        q_entry.translate_issued = true;
        q_entry.address = 0;
      }
    }
  }
}

void CACHE::TranslatingQueues::detect_misses()
{
  do_detect_misses(WQ);
  do_detect_misses(RQ);
  do_detect_misses(PQ);
}

template <typename R>
void CACHE::TranslatingQueues::do_detect_misses(R& queue)
{
  // Find entries that would be ready except that they have not finished translation, move them to the back of the queue
  auto q_it = std::find_if_not(std::begin(queue), std::end(queue), [this](auto x) { return x.event_cycle < this->current_cycle && x.address == 0; });
  std::for_each(std::begin(queue), q_it, [](auto& x) { x.event_cycle = std::numeric_limits<uint64_t>::max(); });
  std::rotate(std::begin(queue), q_it, std::end(queue));
}

template <typename R>
bool CACHE::NonTranslatingQueues::do_add_queue(R& queue, std::size_t queue_size, const PACKET& packet)
{
  assert(packet.address != 0);

  // check occupancy
  if (std::size(queue) >= queue_size) {
    if constexpr (champsim::debug_print) {
      std::cout << " FULL" << std::endl;
    }

    return false; // cannot handle this request
  }

  // Insert the packet ahead of the translation misses
  auto ins_loc = std::find_if(std::begin(queue), std::end(queue), [](auto x) { return x.event_cycle == std::numeric_limits<uint64_t>::max(); });
  auto fwd_pkt = packet;
  fwd_pkt.forward_checked = false;
  fwd_pkt.translate_issued = false;
  fwd_pkt.prefetch_from_this = false;
  fwd_pkt.event_cycle = current_cycle + (warmup ? 0 : HIT_LATENCY);
  queue.insert(ins_loc, fwd_pkt);

  if constexpr (champsim::debug_print) {
    std::cout << " ADDED event_cycle: " << fwd_pkt.event_cycle << std::endl;
  }

  return true;
}

bool CACHE::NonTranslatingQueues::add_rq(const PACKET& packet)
{
  sim_stats.back().RQ_ACCESS++;

  auto fwd_pkt = packet;
  fwd_pkt.fill_this_level = true;
  auto result = do_add_queue(RQ, RQ_SIZE, fwd_pkt);

  if (result)
    sim_stats.back().RQ_TO_CACHE++;
  else
    sim_stats.back().RQ_FULL++;

  return result;
}

bool CACHE::NonTranslatingQueues::add_wq(const PACKET& packet)
{
  sim_stats.back().WQ_ACCESS++;

  auto fwd_pkt = packet;
  fwd_pkt.fill_this_level = true;
  auto result = do_add_queue(WQ, WQ_SIZE, fwd_pkt);

  if (result)
    sim_stats.back().WQ_TO_CACHE++;
  else
    sim_stats.back().WQ_FULL++;

  return result;
}

bool CACHE::NonTranslatingQueues::add_pq(const PACKET& packet)
{
  sim_stats.back().PQ_ACCESS++;
  auto result = do_add_queue(PQ, PQ_SIZE, packet);
  if (result)
    sim_stats.back().PQ_TO_CACHE++;
  else
    sim_stats.back().PQ_FULL++;

  return result;
}

bool CACHE::NonTranslatingQueues::is_ready(const PACKET& pkt) const { return pkt.event_cycle <= current_cycle; }

bool CACHE::TranslatingQueues::is_ready(const PACKET& pkt) const { return NonTranslatingQueues::is_ready(pkt) && pkt.address != 0 && pkt.address != pkt.v_address; }

bool CACHE::NonTranslatingQueues::wq_has_ready() const { return is_ready(WQ.front()); }

bool CACHE::NonTranslatingQueues::rq_has_ready() const { return is_ready(RQ.front()); }

bool CACHE::NonTranslatingQueues::pq_has_ready() const { return is_ready(PQ.front()); }

void CACHE::TranslatingQueues::return_data(const PACKET& packet)
{
  if constexpr (champsim::debug_print) {
    std::cout << "[TRANSLATE] " << __func__ << " instr_id: " << packet.instr_id;
    std::cout << " address: " << std::hex << packet.address;
    std::cout << " data: " << packet.data << std::dec;
    std::cout << " event: " << packet.event_cycle << " current: " << current_cycle << std::endl;
  }

  // Find all packets that match the page of the returned packet
  for (auto& wq_entry : WQ) {
    if ((wq_entry.v_address >> LOG2_PAGE_SIZE) == (packet.v_address >> LOG2_PAGE_SIZE)) {
      wq_entry.address = splice_bits(packet.data, wq_entry.v_address, LOG2_PAGE_SIZE); // translated address
      wq_entry.event_cycle = std::min(wq_entry.event_cycle, current_cycle + (warmup ? 0 : HIT_LATENCY));
    }
  }

  for (auto& rq_entry : RQ) {
    if ((rq_entry.v_address >> LOG2_PAGE_SIZE) == (packet.v_address >> LOG2_PAGE_SIZE)) {
      rq_entry.address = splice_bits(packet.data, rq_entry.v_address, LOG2_PAGE_SIZE); // translated address
      rq_entry.event_cycle = std::min(rq_entry.event_cycle, current_cycle + (warmup ? 0 : HIT_LATENCY));
    }
  }

  for (auto& pq_entry : PQ) {
    if ((pq_entry.v_address >> LOG2_PAGE_SIZE) == (packet.v_address >> LOG2_PAGE_SIZE)) {
      pq_entry.address = splice_bits(packet.data, pq_entry.v_address, LOG2_PAGE_SIZE); // translated address
      pq_entry.event_cycle = std::min(pq_entry.event_cycle, current_cycle + (warmup ? 0 : HIT_LATENCY));
    }
  }
}

void CACHE::NonTranslatingQueues::begin_phase()
{
  roi_stats.emplace_back();
  sim_stats.emplace_back();
}

void CACHE::NonTranslatingQueues::end_phase(unsigned)
{
  roi_stats.back().RQ_ACCESS = sim_stats.back().RQ_ACCESS;
  roi_stats.back().RQ_MERGED = sim_stats.back().RQ_MERGED;
  roi_stats.back().RQ_FULL = sim_stats.back().RQ_FULL;
  roi_stats.back().RQ_TO_CACHE = sim_stats.back().RQ_TO_CACHE;

  roi_stats.back().PQ_ACCESS = sim_stats.back().PQ_ACCESS;
  roi_stats.back().PQ_MERGED = sim_stats.back().PQ_MERGED;
  roi_stats.back().PQ_FULL = sim_stats.back().PQ_FULL;
  roi_stats.back().PQ_TO_CACHE = sim_stats.back().PQ_TO_CACHE;

  roi_stats.back().WQ_ACCESS = sim_stats.back().WQ_ACCESS;
  roi_stats.back().WQ_MERGED = sim_stats.back().WQ_MERGED;
  roi_stats.back().WQ_FULL = sim_stats.back().WQ_FULL;
  roi_stats.back().WQ_TO_CACHE = sim_stats.back().WQ_TO_CACHE;
  roi_stats.back().WQ_FORWARD = sim_stats.back().WQ_FORWARD;
}
