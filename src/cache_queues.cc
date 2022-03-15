#include "cache.h"
#include "util.h"
#include "vmem.h"

void CACHE::NonTranslatingQueues::operate()
{
  check_collision();
}

void CACHE::TranslatingQueues::operate()
{
  NonTranslatingQueues::operate();
  issue_translation();
}

void CACHE::NonTranslatingQueues::check_collision()
{
  std::size_t write_shamt = match_offset_bits ? 0 : OFFSET_BITS;
  std::size_t read_shamt = OFFSET_BITS;

  for (auto wq_it = std::find_if(std::begin(WQ), std::end(WQ), std::not_fn(&PACKET::forward_checked)); wq_it != std::end(WQ);) {
    if (auto found = std::find_if(std::begin(WQ), wq_it, eq_addr<PACKET>(wq_it->address, write_shamt)); found != wq_it) {
      WQ_MERGED++;
      wq_it = WQ.erase(wq_it);
    } else {
      wq_it->forward_checked = true;
      ++wq_it;
    }
  }

  for (auto rq_it = std::find_if(std::begin(RQ), std::end(RQ), std::not_fn(&PACKET::forward_checked)); rq_it != std::end(RQ);) {
    if (auto found_wq = std::find_if(std::begin(WQ), std::end(WQ), eq_addr<PACKET>(rq_it->address, write_shamt)); found_wq != std::end(WQ)) {
      rq_it->data = found_wq->data;
      for (auto ret : rq_it->to_return)
        ret->return_data(*rq_it);

      WQ_FORWARD++;
      rq_it = RQ.erase(rq_it);
    } else if (auto found_rq = std::find_if(std::begin(RQ), rq_it, eq_addr<PACKET>(rq_it->address, read_shamt)); found_rq != rq_it) {
      packet_dep_merge(found_rq->lq_index_depend_on_me, rq_it->lq_index_depend_on_me);
      packet_dep_merge(found_rq->sq_index_depend_on_me, rq_it->sq_index_depend_on_me);
      packet_dep_merge(found_rq->instr_depend_on_me, rq_it->instr_depend_on_me);
      packet_dep_merge(found_rq->to_return, rq_it->to_return);

      RQ_MERGED++;
      rq_it = RQ.erase(rq_it);
    } else {
      rq_it->forward_checked = true;
      ++rq_it;
    }
  }

  for (auto pq_it = std::find_if(std::begin(PQ), std::end(PQ), std::not_fn(&PACKET::forward_checked)); pq_it != std::end(PQ);) {
    if (auto found_wq = std::find_if(std::begin(WQ), std::end(WQ), eq_addr<PACKET>(pq_it->address, write_shamt)); found_wq != std::end(WQ)) {
      pq_it->data = found_wq->data;
      for (auto ret : pq_it->to_return)
        ret->return_data(*pq_it);

      WQ_FORWARD++;
      pq_it = PQ.erase(pq_it);
    } else if (auto found = std::find_if(std::begin(PQ), pq_it, eq_addr<PACKET>(pq_it->address, read_shamt)); found != pq_it) {
      found->fill_level = std::min(found->fill_level, pq_it->fill_level);
      packet_dep_merge(found->to_return, pq_it->to_return);

      PQ_MERGED++;
      pq_it = PQ.erase(pq_it);
    } else {
      pq_it->forward_checked = true;
      ++pq_it;
    }
  }
}

// virtual address space prefetching
constexpr uint64_t TRANSLATION_LATENCY = 2;
extern VirtualMemory vmem;
void CACHE::TranslatingQueues::issue_translation()
{
  for (auto &wq_entry : WQ) {
    if (!wq_entry.translate_issued && wq_entry.address == wq_entry.v_address) {
      wq_entry.address = vmem.va_to_pa(wq_entry.cpu, wq_entry.v_address).first;
      //auto success = lower_level->add_rq(wq_entry);
      //if (success) {
      wq_entry.translate_issued = true;
      wq_entry.event_cycle += TRANSLATION_LATENCY;
      //}
    }
  }
  std::sort(std::begin(WQ), std::end(WQ), min_event_cycle<PACKET>{});

  for (auto &rq_entry : RQ) {
    if (!rq_entry.translate_issued && rq_entry.address == rq_entry.v_address) {
      rq_entry.address = vmem.va_to_pa(rq_entry.cpu, rq_entry.v_address).first;
      //auto success = lower_level->add_rq(rq_entry);
      //if (success) {
      rq_entry.translate_issued = true;
      rq_entry.event_cycle += TRANSLATION_LATENCY;
      //}
    }
  }
  std::sort(std::begin(RQ), std::end(RQ), min_event_cycle<PACKET>{});

  for (auto &pq_entry : PQ) {
    if (!pq_entry.translate_issued && pq_entry.address == pq_entry.v_address) {
      pq_entry.address = vmem.va_to_pa(pq_entry.cpu, pq_entry.v_address).first;
      //auto success = lower_level->add_rq(pq_entry);
      //if (success) {
      pq_entry.translate_issued = true;
      pq_entry.event_cycle += TRANSLATION_LATENCY;
      //}
    }
  }
  std::sort(std::begin(PQ), std::end(PQ), min_event_cycle<PACKET>{});
}

bool CACHE::NonTranslatingQueues::add_rq(const PACKET &packet)
{
  assert(packet.address != 0);
  RQ_ACCESS++;

  DP(if (warmup_complete[packet.cpu]) {
    std::cout << "[" << NAME << "_RQ] " << __func__ << " instr_id: " << packet.instr_id << " address: " << std::hex << (packet.address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet.address << " v_address: " << packet.v_address << std::dec << " type: " << +packet.type
              << " occupancy: " << RQ.size();
  })

  // check occupancy
  if (std::size(RQ) >= RQ_SIZE) {
    RQ_FULL++;

    DP(if (warmup_complete[packet.cpu]) std::cout << " FULL" << std::endl;)

    return false; // cannot handle this request
  }

  // if there is no duplicate, add it to RQ
  RQ.push_back(packet);

  DP(if (warmup_complete[packet.cpu]) std::cout << " ADDED" << std::endl;)

  RQ_TO_CACHE++;
  return true;
}

bool CACHE::NonTranslatingQueues::add_wq(const PACKET &packet)
{
  WQ_ACCESS++;

  DP(if (warmup_complete[packet.cpu]) {
    std::cout << "[" << NAME << "_WQ] " << __func__ << " instr_id: " << packet.instr_id << " address: " << std::hex << (packet.address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet.address << " v_address: " << packet.v_address << std::dec << " type: " << +packet.type
              << " occupancy: " << WQ.size();
  })

  // Check for room in the queue
  if (std::size(WQ) >= WQ_SIZE) {
    DP(if (warmup_complete[packet.cpu]) std::cout << " FULL" << std::endl;)

    ++WQ_FULL;
    return false;
  }

  // if there is no duplicate, add it to the write queue
  WQ.push_back(packet);

  DP(if (warmup_complete[packet.cpu]) std::cout << " ADDED" << std::endl;)

  WQ_TO_CACHE++;
  WQ_ACCESS++;

  return true;
}

bool CACHE::NonTranslatingQueues::add_pq(const PACKET &packet)
{
  assert(packet.address != 0);
  PQ_ACCESS++;

  DP(if (warmup_complete[packet.cpu]) {
    std::cout << "[" << NAME << "_WQ] " << __func__ << " instr_id: " << packet.instr_id << " address: " << std::hex << (packet.address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet.address << " v_address: " << packet.v_address << std::dec << " type: " << +packet.type
              << " occupancy: " << PQ.size();
  })

  // check occupancy
  if (std::size(PQ) >= PQ_SIZE) {

    DP(if (warmup_complete[packet.cpu]) std::cout << " FULL" << std::endl;)

    PQ_FULL++;
    return false; // cannot handle this request
  }

  // if there is no duplicate, add it to PQ
  PQ.push_back(packet);

  DP(if (warmup_complete[packet.cpu]) std::cout << " ADDED" << std::endl;)

  PQ_TO_CACHE++;
  return true;
}

bool CACHE::NonTranslatingQueues::wq_has_ready() const
{
  return WQ.front().event_cycle <= current_cycle;
}

bool CACHE::NonTranslatingQueues::rq_has_ready() const
{
  return RQ.front().event_cycle <= current_cycle;
}

bool CACHE::NonTranslatingQueues::pq_has_ready() const
{
  return PQ.front().event_cycle <= current_cycle;
}

bool CACHE::TranslatingQueues::wq_has_ready() const
{
  return WQ.front().event_cycle <= current_cycle && WQ.front().address != WQ.front().v_address;
}

bool CACHE::TranslatingQueues::rq_has_ready() const
{
  return RQ.front().event_cycle <= current_cycle && RQ.front().address != RQ.front().v_address;
}

bool CACHE::TranslatingQueues::pq_has_ready() const
{
  return PQ.front().event_cycle <= current_cycle && PQ.front().address != PQ.front().v_address;
}

void CACHE::TranslatingQueues::return_data(const PACKET &packet)
{
  eq_addr<PACKET> checker{packet.address, OFFSET_BITS};
  for (auto &wq_entry : WQ) {
    if (checker(wq_entry)) {
      wq_entry.address = splice_bits(packet.data, wq_entry.v_address, LOG2_PAGE_SIZE); // translated address
    }
  }
  for (auto &rq_entry : RQ) {
    if (checker(rq_entry)) {
      rq_entry.address = splice_bits(packet.data, rq_entry.v_address, LOG2_PAGE_SIZE); // translated address
    }
  }
  for (auto &pq_entry : PQ) {
    if (checker(pq_entry)) {
      pq_entry.address = splice_bits(packet.data, pq_entry.v_address, LOG2_PAGE_SIZE); // translated address
    }
  }
}
