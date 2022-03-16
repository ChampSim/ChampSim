#include "cache.h"
#include "util.h"

extern uint8_t warmup_complete[NUM_CPUS];

void CACHE::NonTranslatingQueues::operate()
{
  check_collision();
}

void CACHE::TranslatingQueues::operate()
{
  NonTranslatingQueues::operate();
  issue_translation();
  detect_misses();
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
      packet_dep_merge(found->to_return, pq_it->to_return);

      PQ_MERGED++;
      pq_it = PQ.erase(pq_it);
    } else {
      pq_it->forward_checked = true;
      ++pq_it;
    }
  }
}

void CACHE::TranslatingQueues::issue_translation()
{
  for (auto &wq_entry : WQ) {
    if (!wq_entry.translate_issued && wq_entry.address == wq_entry.v_address) {
      auto fwd_pkt = wq_entry;
      fwd_pkt.to_return = {this};
      auto success = lower_level->add_rq(fwd_pkt);
      if (success) {
        DP(if (warmup_complete[wq_entry.cpu]) {
          std::cout << "[TRANSLATE] " << __func__ << " instr_id: " << wq_entry.instr_id;
          std::cout << " address: "  << std::hex << wq_entry.address << " v_address: " << wq_entry.v_address << std::dec;
          std::cout << " type: " << +wq_entry.type << " occupancy: " << std::size(WQ) << std::endl;
        })

        wq_entry.translate_issued = true;
        wq_entry.address = 0;
      }
    }
  }

  for (auto &rq_entry : RQ) {
    if (!rq_entry.translate_issued && rq_entry.address == rq_entry.v_address) {
      auto fwd_pkt = rq_entry;
      fwd_pkt.to_return = {this};
      auto success = lower_level->add_rq(fwd_pkt);
      if (success) {
        DP(if (warmup_complete[rq_entry.cpu]) {
          std::cout << "[TRANSLATE] " << __func__ << " instr_id: " << rq_entry.instr_id;
          std::cout << " address: " << std::hex << rq_entry.address << " v_address: " << rq_entry.v_address << std::dec;
          std::cout << " type: " << +rq_entry.type << " occupancy: " << std::size(RQ) << std::endl;
        })

        rq_entry.translate_issued = true;
        rq_entry.address = 0;
      }
    }
  }

  for (auto &pq_entry : PQ) {
    if (!pq_entry.translate_issued && pq_entry.address == pq_entry.v_address) {
      auto fwd_pkt = pq_entry;
      fwd_pkt.to_return = {this};
      auto success = lower_level->add_rq(fwd_pkt);
      if (success) {
        DP(if (warmup_complete[pq_entry.cpu]) {
          std::cout << "[TRANSLATE] " << __func__ << " instr_id: " << pq_entry.instr_id;
          std::cout << " address: " << std::hex << pq_entry.address << " v_address: " << pq_entry.v_address << std::dec;
          std::cout << " type: " << +pq_entry.type << " occupancy: " << std::size(PQ) << std::endl;
        })

        pq_entry.translate_issued = true;
        pq_entry.address = 0;
      }
    }
  }
}

void CACHE::TranslatingQueues::detect_misses()
{
  // Find entries that would be ready except that they have not finished translation, move them to the back of the queue
  auto wq_it = std::find_if_not(std::begin(WQ), std::end(WQ), [this](auto x){ return x.event_cycle < this->current_cycle && x.address == 0; });
  std::for_each(std::begin(WQ), wq_it, [](auto &x){ x.event_cycle = std::numeric_limits<uint64_t>::max(); });
  std::rotate(std::begin(WQ), wq_it, std::end(WQ));

  auto rq_it = std::find_if_not(std::begin(RQ), std::end(RQ), [this](auto x){ return x.event_cycle < this->current_cycle && x.address == 0; });
  std::for_each(std::begin(RQ), rq_it, [](auto &x){ x.event_cycle = std::numeric_limits<uint64_t>::max(); });
  std::rotate(std::begin(RQ), rq_it, std::end(RQ));

  auto pq_it = std::find_if_not(std::begin(PQ), std::end(PQ), [this](auto x){ return x.event_cycle < this->current_cycle && x.address == 0; });
  std::for_each(std::begin(PQ), pq_it, [](auto &x){ x.event_cycle = std::numeric_limits<uint64_t>::max(); });
  std::rotate(std::begin(PQ), pq_it, std::end(PQ));
}

bool CACHE::NonTranslatingQueues::add_rq(const PACKET &packet)
{
  assert(packet.address != 0);
  RQ_ACCESS++;

  // check occupancy
  if (std::size(RQ) >= RQ_SIZE) {
    RQ_FULL++;

    DP(if (warmup_complete[packet.cpu]) std::cout << " FULL" << std::endl;)

    return false; // cannot handle this request
  }

  // Insert the packet ahead of the translation misses
  auto ins_loc = std::find_if(std::begin(RQ), std::end(RQ), [](auto x){ return x.event_cycle == std::numeric_limits<uint64_t>::max(); });
  auto fwd_pkt = packet;
  fwd_pkt.forward_checked = false;
  fwd_pkt.translate_issued = false;
  fwd_pkt.event_cycle = current_cycle + warmup_complete[packet.cpu] ? HIT_LATENCY : 0;
  RQ.insert(ins_loc, fwd_pkt);

  DP(if (warmup_complete[packet.cpu]) std::cout << " ADDED" << std::endl;)

  RQ_TO_CACHE++;
  return true;
}

bool CACHE::NonTranslatingQueues::add_wq(const PACKET &packet)
{
  WQ_ACCESS++;

  // Check for room in the queue
  if (std::size(WQ) >= WQ_SIZE) {
    DP(if (warmup_complete[packet.cpu]) std::cout << " FULL" << std::endl;)

    ++WQ_FULL;
    return false;
  }

  // Insert the packet ahead of the translation misses
  auto ins_loc = std::find_if(std::begin(WQ), std::end(WQ), [](auto x){ return x.event_cycle == std::numeric_limits<uint64_t>::max(); });
  auto fwd_pkt = packet;
  fwd_pkt.forward_checked = false;
  fwd_pkt.translate_issued = false;
  fwd_pkt.event_cycle = current_cycle + warmup_complete[packet.cpu] ? HIT_LATENCY : 0;
  WQ.insert(ins_loc, fwd_pkt);

  DP(if (warmup_complete[packet.cpu]) std::cout << " ADDED" << std::endl;)

  WQ_TO_CACHE++;
  WQ_ACCESS++;

  return true;
}

bool CACHE::NonTranslatingQueues::add_pq(const PACKET &packet)
{
  assert(packet.address != 0);
  PQ_ACCESS++;

  // check occupancy
  if (std::size(PQ) >= PQ_SIZE) {

    DP(if (warmup_complete[packet.cpu]) std::cout << " FULL" << std::endl;)

    PQ_FULL++;
    return false; // cannot handle this request
  }

  // Insert the packet ahead of the translation misses
  auto ins_loc = std::find_if(std::begin(PQ), std::end(PQ), [](auto x){ return x.event_cycle == std::numeric_limits<uint64_t>::max(); });
  auto fwd_pkt = packet;
  fwd_pkt.forward_checked = false;
  fwd_pkt.translate_issued = false;
  fwd_pkt.event_cycle = current_cycle + warmup_complete[packet.cpu] ? HIT_LATENCY : 0;
  PQ.insert(ins_loc, fwd_pkt);

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
  return NonTranslatingQueues::wq_has_ready() && WQ.front().address != 0 && WQ.front().address != WQ.front().v_address;
}

bool CACHE::TranslatingQueues::rq_has_ready() const
{
  return NonTranslatingQueues::rq_has_ready() && RQ.front().address != 0 && RQ.front().address != RQ.front().v_address;
}

bool CACHE::TranslatingQueues::pq_has_ready() const
{
  return NonTranslatingQueues::pq_has_ready() && PQ.front().address != 0 && PQ.front().address != PQ.front().v_address;
}

void CACHE::TranslatingQueues::return_data(const PACKET &packet)
{
  DP(if (warmup_complete[packet.cpu]) {
    std::cout << "[TRANSLATE] " << __func__ << " instr_id: " << packet.instr_id;
    std::cout << " address: " << std::hex << packet.address;
    std::cout << " data: " << packet.data << std::dec;
    std::cout << " event: " << packet.event_cycle << " current: " << current_cycle << std::endl;
  });

  // Find all packets that match the page of the returned packet
  for (auto &wq_entry : WQ) {
    if ((wq_entry.v_address >> LOG2_PAGE_SIZE) == (packet.v_address >> LOG2_PAGE_SIZE)) {
      wq_entry.address = splice_bits(packet.data, wq_entry.v_address, LOG2_PAGE_SIZE); // translated address
      wq_entry.event_cycle = std::min(wq_entry.event_cycle, current_cycle + (warmup_complete[wq_entry.cpu] ? HIT_LATENCY : 0));
    }
  }

  for (auto &rq_entry : RQ) {
    if ((rq_entry.v_address >> LOG2_PAGE_SIZE) == (packet.v_address >> LOG2_PAGE_SIZE)) {
      rq_entry.address = splice_bits(packet.data, rq_entry.v_address, LOG2_PAGE_SIZE); // translated address
      rq_entry.event_cycle = std::min(rq_entry.event_cycle, current_cycle + (warmup_complete[rq_entry.cpu] ? HIT_LATENCY : 0));
    }
  }

  for (auto &pq_entry : PQ) {
    if ((pq_entry.v_address >> LOG2_PAGE_SIZE) == (packet.v_address >> LOG2_PAGE_SIZE)) {
      pq_entry.address = splice_bits(packet.data, pq_entry.v_address, LOG2_PAGE_SIZE); // translated address
      pq_entry.event_cycle = std::min(pq_entry.event_cycle, current_cycle + (warmup_complete[pq_entry.cpu] ? HIT_LATENCY : 0));
    }
  }
}
