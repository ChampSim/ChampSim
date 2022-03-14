#include "cache.h"

#include <algorithm>
#include <iomanip>
#include <iterator>
#include <numeric>

#include "champsim.h"
#include "champsim_constants.h"
#include "util.h"
#include "vmem.h"

#ifndef SANITY_CHECK
#define NDEBUG
#endif

extern VirtualMemory vmem;

void CACHE::handle_fill()
{
  while (writes_available_this_cycle > 0) {
    auto fill_mshr = MSHR.begin();
    if (fill_mshr == std::end(MSHR) || fill_mshr->event_cycle > current_cycle)
      return;

    // find victim
    uint32_t set = get_set(fill_mshr->address);

    auto set_begin = std::next(std::begin(block), set * NUM_WAY);
    auto set_end = std::next(set_begin, NUM_WAY);
    auto first_inv = std::find_if_not(set_begin, set_end, is_valid<BLOCK>());
    uint32_t way = std::distance(set_begin, first_inv);
    if (way == NUM_WAY)
      way = impl_replacement_find_victim(fill_mshr->cpu, fill_mshr->instr_id, set, &block.data()[set * NUM_WAY], fill_mshr->ip, fill_mshr->address,
                                         fill_mshr->type);

    bool success = filllike_miss(set, way, *fill_mshr);
    if (!success)
      return;

    for (auto ret : fill_mshr->to_return)
      ret->return_data(*fill_mshr);

    MSHR.erase(fill_mshr);
    writes_available_this_cycle--;
  }
}

void CACHE::handle_writeback()
{
  while (writes_available_this_cycle > 0 && !std::empty(WQ) && WQ.front().event_cycle <= current_cycle) {
    // handle the oldest entry
    PACKET& handle_pkt = WQ.front();

    // access cache
    uint32_t set = get_set(handle_pkt.address);
    uint32_t way = get_way(handle_pkt.address, set);

    BLOCK& fill_block = block[set * NUM_WAY + way];

    if (way < NUM_WAY) // HIT
    {
      impl_replacement_update_state(handle_pkt.cpu, set, way, fill_block.address, handle_pkt.ip, 0, handle_pkt.type, 1);

      // COLLECT STATS
      sim_stats.back().hits[handle_pkt.cpu][handle_pkt.type]++;

      // mark dirty
      fill_block.dirty = 1;
    } else // MISS
    {
      bool success;
      if (handle_pkt.type == RFO && handle_pkt.to_return.empty()) {
        success = readlike_miss(handle_pkt);
      } else {
        // find victim
        auto set_begin = std::next(std::begin(block), set * NUM_WAY);
        auto set_end = std::next(set_begin, NUM_WAY);
        auto first_inv = std::find_if_not(set_begin, set_end, is_valid<BLOCK>());
        way = std::distance(set_begin, first_inv);
        if (way == NUM_WAY)
          way = impl_replacement_find_victim(handle_pkt.cpu, handle_pkt.instr_id, set, &block.data()[set * NUM_WAY], handle_pkt.ip, handle_pkt.address,
                                             handle_pkt.type);

        success = filllike_miss(set, way, handle_pkt);
      }

      if (!success)
        return;
    }

    // remove this entry from WQ
    writes_available_this_cycle--;
    WQ.pop_front();
  }
}

void CACHE::handle_read()
{
  while (reads_available_this_cycle > 0 && !std::empty(RQ) && RQ.front().event_cycle <= current_cycle) {
    // handle the oldest entry
    PACKET& handle_pkt = RQ.front();

    // A (hopefully temporary) hack to know whether to send the evicted paddr or
    // vaddr to the prefetcher
    ever_seen_data |= (handle_pkt.v_address != handle_pkt.ip);

    uint32_t set = get_set(handle_pkt.address);
    uint32_t way = get_way(handle_pkt.address, set);

    if (way < NUM_WAY) // HIT
    {
      readlike_hit(set, way, handle_pkt);
    } else {
      bool success = readlike_miss(handle_pkt);
      if (!success)
        return;
    }

    // remove this entry from RQ
    RQ.pop_front();
    reads_available_this_cycle--;
  }
}

void CACHE::handle_prefetch()
{
  while (reads_available_this_cycle > 0 && !std::empty(PQ) && PQ.front().event_cycle <= current_cycle) {
    // handle the oldest entry
    PACKET& handle_pkt = PQ.front();

    uint32_t set = get_set(handle_pkt.address);
    uint32_t way = get_way(handle_pkt.address, set);

    if (way < NUM_WAY) // HIT
    {
      readlike_hit(set, way, handle_pkt);
    } else {
      bool success = readlike_miss(handle_pkt);
      if (!success)
        return;
    }

    // remove this entry from PQ
    PQ.pop_front();
    reads_available_this_cycle--;
  }
}

void CACHE::check_collision()
{
  for (auto wq_it = std::find_if(std::begin(WQ), std::end(WQ), std::not_fn(&PACKET::forward_checked)); wq_it != std::end(WQ);) {
    if (auto found = std::find_if(std::begin(WQ), wq_it, eq_addr<PACKET>(wq_it->address, match_offset_bits ? 0 : OFFSET_BITS)); found != wq_it) {
      sim_stats.back().WQ_MERGED++;
      wq_it = WQ.erase(wq_it);
    } else {
      wq_it->forward_checked = true;
      ++wq_it;
    }
  }

  for (auto rq_it = std::find_if(std::begin(RQ), std::end(RQ), std::not_fn(&PACKET::forward_checked)); rq_it != std::end(RQ);) {
    if (auto found_wq = std::find_if(std::begin(WQ), std::end(WQ), eq_addr<PACKET>(rq_it->address, match_offset_bits ? 0 : OFFSET_BITS));
        found_wq != std::end(WQ)) {
      rq_it->data = found_wq->data;
      for (auto ret : rq_it->to_return)
        ret->return_data(*rq_it);

      sim_stats.back().WQ_FORWARD++;
      rq_it = RQ.erase(rq_it);
    } else if (auto found_rq = std::find_if(std::begin(RQ), rq_it, eq_addr<PACKET>(rq_it->address, OFFSET_BITS)); found_rq != rq_it) {
      packet_dep_merge(found_rq->lq_index_depend_on_me, rq_it->lq_index_depend_on_me);
      packet_dep_merge(found_rq->sq_index_depend_on_me, rq_it->sq_index_depend_on_me);
      packet_dep_merge(found_rq->instr_depend_on_me, rq_it->instr_depend_on_me);
      packet_dep_merge(found_rq->to_return, rq_it->to_return);

      sim_stats.back().RQ_MERGED++;
      rq_it = RQ.erase(rq_it);
    } else {
      rq_it->forward_checked = true;
      ++rq_it;
    }
  }

  for (auto pq_it = std::find_if(std::begin(PQ), std::end(PQ), std::not_fn(&PACKET::forward_checked)); pq_it != std::end(PQ);) {
    if (auto found_wq = std::find_if(std::begin(WQ), std::end(WQ), eq_addr<PACKET>(pq_it->address, match_offset_bits ? 0 : OFFSET_BITS));
        found_wq != std::end(WQ)) {
      pq_it->data = found_wq->data;
      for (auto ret : pq_it->to_return)
        ret->return_data(*pq_it);

      sim_stats.back().WQ_FORWARD++;
      pq_it = PQ.erase(pq_it);
    } else if (auto found = std::find_if(std::begin(PQ), pq_it, eq_addr<PACKET>(pq_it->address, OFFSET_BITS)); found != pq_it) {
      found->fill_level = std::min(found->fill_level, pq_it->fill_level);
      packet_dep_merge(found->to_return, pq_it->to_return);

      sim_stats.back().PQ_MERGED++;
      pq_it = PQ.erase(pq_it);
    } else {
      pq_it->forward_checked = true;
      ++pq_it;
    }
  }
}

void CACHE::readlike_hit(std::size_t set, std::size_t way, PACKET& handle_pkt)
{
  DP(if (!warmup) {
    std::cout << "[" << NAME << "] " << __func__ << " hit";
    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << (handle_pkt.address >> OFFSET_BITS);
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address << std::dec;
    std::cout << " type: " << +handle_pkt.type;
    std::cout << " cycle: " << current_cycle << std::endl;
  });

  BLOCK& hit_block = block[set * NUM_WAY + way];

  handle_pkt.data = hit_block.data;

  // update prefetcher on load instruction
  if (should_activate_prefetcher(handle_pkt.type) && handle_pkt.pf_origin_level < fill_level) {
    cpu = handle_pkt.cpu;
    uint64_t pf_base_addr = (virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);
    handle_pkt.pf_metadata = impl_prefetcher_cache_operate(pf_base_addr, handle_pkt.ip, 1, handle_pkt.type, handle_pkt.pf_metadata);
  }

  // update replacement policy
  impl_replacement_update_state(handle_pkt.cpu, set, way, hit_block.address, handle_pkt.ip, 0, handle_pkt.type, 1);

  // COLLECT STATS
  sim_stats.back().hits[handle_pkt.cpu][handle_pkt.type]++;

  for (auto ret : handle_pkt.to_return)
    ret->return_data(handle_pkt);

    // update prefetch stats and reset prefetch bit
    if (hit_block.prefetch) {
        sim_stats.back().pf_useful++;
        hit_block.prefetch = 0;
    }
}

bool CACHE::readlike_miss(PACKET& handle_pkt)
{
  DP(if (!warmup) {
    std::cout << "[" << NAME << "] " << __func__ << " miss";
    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << (handle_pkt.address >> OFFSET_BITS);
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address << std::dec;
    std::cout << " type: " << +handle_pkt.type;
    std::cout << " cycle: " << current_cycle << std::endl;
  });

  // check mshr
  auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(handle_pkt.address, OFFSET_BITS));
  bool mshr_full = (MSHR.size() == MSHR_SIZE);

  if (mshr_entry != MSHR.end()) // miss already inflight
  {
    // update fill location
    mshr_entry->fill_level = std::min(mshr_entry->fill_level, handle_pkt.fill_level);

    packet_dep_merge(mshr_entry->lq_index_depend_on_me, handle_pkt.lq_index_depend_on_me);
    packet_dep_merge(mshr_entry->sq_index_depend_on_me, handle_pkt.sq_index_depend_on_me);
    packet_dep_merge(mshr_entry->instr_depend_on_me, handle_pkt.instr_depend_on_me);
    packet_dep_merge(mshr_entry->to_return, handle_pkt.to_return);

    if (mshr_entry->type == PREFETCH && handle_pkt.type != PREFETCH) {
      // Mark the prefetch as useful
      if (mshr_entry->pf_origin_level == fill_level)
                sim_stats.back().pf_useful++;

      uint64_t prior_event_cycle = mshr_entry->event_cycle;
      *mshr_entry = handle_pkt;

      // in case request is already returned, we should keep event_cycle
      mshr_entry->event_cycle = prior_event_cycle;
    }
  } else {
    if (mshr_full)  // not enough MSHR resource
      return false; // TODO should we allow prefetches anyway if they will not
                    // be filled to this level?

    auto fwd_pkt = handle_pkt;

    if (fwd_pkt.fill_level <= fill_level)
      fwd_pkt.to_return = {this};
    else
      fwd_pkt.to_return.clear();

    bool success;
    if (prefetch_as_load || handle_pkt.type != PREFETCH)
      success = lower_level->add_rq(fwd_pkt);
    else
      success = lower_level->add_pq(fwd_pkt);

    if (!success)
      return false;

    // Allocate an MSHR
    if (handle_pkt.fill_level <= fill_level) {
      auto it = MSHR.insert(std::end(MSHR), handle_pkt);
      it->cycle_enqueued = current_cycle;
      it->event_cycle = std::numeric_limits<uint64_t>::max();
    }
  }

  // update prefetcher on load instructions and prefetches from upper levels
  if (should_activate_prefetcher(handle_pkt.type) && handle_pkt.pf_origin_level < fill_level) {
    cpu = handle_pkt.cpu;
    uint64_t pf_base_addr = (virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);
    handle_pkt.pf_metadata = impl_prefetcher_cache_operate(pf_base_addr, handle_pkt.ip, 0, handle_pkt.type, handle_pkt.pf_metadata);
  }

  return true;
}

bool CACHE::filllike_miss(std::size_t set, std::size_t way, PACKET& handle_pkt)
{
  DP(if (!warmup) {
    std::cout << "[" << NAME << "] " << __func__ << " miss";
    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << (handle_pkt.address >> OFFSET_BITS);
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address << std::dec;
    std::cout << " type: " << +handle_pkt.type;
    std::cout << " cycle: " << current_cycle << std::endl;
  });

  bool bypass = (way == NUM_WAY);
#ifndef LLC_BYPASS
  assert(!bypass);
#endif
  assert(handle_pkt.type != WRITEBACK || !bypass);

  BLOCK& fill_block = block[set * NUM_WAY + way];
  bool evicting_dirty = !bypass && (lower_level != NULL) && fill_block.dirty;
  uint64_t evicting_address = 0;

  if (!bypass) {
    if (evicting_dirty) {
      PACKET writeback_packet;

      writeback_packet.fill_level = lower_level->fill_level;
      writeback_packet.cpu = handle_pkt.cpu;
      writeback_packet.address = fill_block.address;
      writeback_packet.data = fill_block.data;
      writeback_packet.instr_id = handle_pkt.instr_id;
      writeback_packet.ip = 0;
      writeback_packet.type = WRITEBACK;

      auto success = lower_level->add_wq(writeback_packet);
      if (!success)
        return false;
    }

    if (ever_seen_data)
      evicting_address = fill_block.address & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);
    else
      evicting_address = fill_block.v_address & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);

        if (fill_block.prefetch)
            sim_stats.back().pf_useless++;

        if (handle_pkt.type == PREFETCH)
            sim_stats.back().pf_fill++;

    fill_block.valid = true;
    fill_block.prefetch = (handle_pkt.type == PREFETCH && handle_pkt.pf_origin_level == fill_level);
    fill_block.dirty = (handle_pkt.type == WRITEBACK || (handle_pkt.type == RFO && handle_pkt.to_return.empty()));
    fill_block.address = handle_pkt.address;
    fill_block.v_address = handle_pkt.v_address;
    fill_block.data = handle_pkt.data;
    fill_block.ip = handle_pkt.ip;
    fill_block.cpu = handle_pkt.cpu;
    fill_block.instr_id = handle_pkt.instr_id;
  }

    if (handle_pkt.cycle_enqueued != 0)
        sim_stats.back().total_miss_latency += current_cycle - handle_pkt.cycle_enqueued;

  // update prefetcher
  cpu = handle_pkt.cpu;
  handle_pkt.pf_metadata =
      impl_prefetcher_cache_fill((virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS), set, way,
                                 handle_pkt.type == PREFETCH, evicting_address, handle_pkt.pf_metadata);

  // update replacement policy
  impl_replacement_update_state(handle_pkt.cpu, set, way, handle_pkt.address, handle_pkt.ip, 0, handle_pkt.type, 0);

    // COLLECT STATS
    sim_stats.back().misses[handle_pkt.cpu][handle_pkt.type]++;

  return true;
}

void CACHE::operate()
{
  check_collision();
  operate_writes();
  operate_reads();

  impl_prefetcher_cycle_operate();
}

void CACHE::operate_writes()
{
  // perform all writes
  writes_available_this_cycle = MAX_WRITE;
  handle_fill();
  handle_writeback();
}

void CACHE::operate_reads()
{
  // perform all reads
  reads_available_this_cycle = MAX_READ;
  handle_read();
  va_translate_prefetches();
  handle_prefetch();
}

uint32_t CACHE::get_set(uint64_t address) { return ((address >> OFFSET_BITS) & bitmask(lg2(NUM_SET))); }

uint32_t CACHE::get_way(uint64_t address, uint32_t set)
{
  auto begin = std::next(block.begin(), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);
  return std::distance(begin, std::find_if(begin, end, eq_addr<BLOCK>(address, OFFSET_BITS)));
}

int CACHE::invalidate_entry(uint64_t inval_addr)
{
  uint32_t set = get_set(inval_addr);
  uint32_t way = get_way(inval_addr, set);

  if (way < NUM_WAY)
    block[set * NUM_WAY + way].valid = 0;

  return way;
}

bool CACHE::add_rq(const PACKET& packet)
{
  assert(packet.address != 0);
  sim_stats.back().RQ_ACCESS++;

  DP(if (!warmup) {
    std::cout << "[" << NAME << "_RQ] " << __func__ << " instr_id: " << packet.instr_id << " address: " << std::hex << (packet.address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet.address << " v_address: " << packet.v_address << std::dec << " type: " << +packet.type << " occupancy: " << RQ.size();
  })

  // check occupancy
  if (std::size(RQ) >= RQ_SIZE) {
    sim_stats.back().RQ_FULL++;

    DP(if (!warmup) std::cout << " FULL" << std::endl;)

    return false; // cannot handle this request
  }

  // if there is no duplicate, add it to RQ
  RQ.push_back(packet);
  RQ.back().forward_checked = false;
  RQ.back().event_cycle = current_cycle + (warmup ? 0 : HIT_LATENCY);

  DP(if (!warmup) std::cout << " ADDED" << std::endl;)

  sim_stats.back().RQ_TO_CACHE++;
  return true;
}

bool CACHE::add_wq(const PACKET& packet)
{
  sim_stats.back().WQ_ACCESS++;

  DP(if (!warmup) {
    std::cout << "[" << NAME << "_WQ] " << __func__ << " instr_id: " << packet.instr_id << " address: " << std::hex << (packet.address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet.address << " v_address: " << packet.v_address << std::dec << " type: " << +packet.type << " occupancy: " << WQ.size();
  })

  // Check for room in the queue
  if (std::size(WQ) >= WQ_SIZE) {
    DP(if (!warmup) std::cout << " FULL" << std::endl;)

    ++sim_stats.back().WQ_FULL;
    return false;
  }

  // if there is no duplicate, add it to the write queue
  WQ.push_back(packet);
  WQ.back().forward_checked = false;
  WQ.back().event_cycle = current_cycle + (warmup ? 0 : HIT_LATENCY);

    DP( if (!warmup) std::cout << " ADDED" << std::endl; )

    sim_stats.back().WQ_TO_CACHE++;
    sim_stats.back().WQ_ACCESS++;

  return true;
}

int CACHE::prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
    sim_stats.back().pf_requested++;

  PACKET pf_packet;
  pf_packet.type = PREFETCH;
  pf_packet.fill_level = (fill_this_level ? fill_level : lower_level->fill_level);
  pf_packet.pf_origin_level = fill_level;
  pf_packet.pf_metadata = prefetch_metadata;
  pf_packet.cpu = cpu;
  pf_packet.address = pf_addr;
  pf_packet.v_address = virtual_prefetch ? pf_addr : 0;

  if (virtual_prefetch) {
    if (std::size(VAPQ) < PQ_SIZE) {
      pf_packet.event_cycle = current_cycle + VA_PREFETCH_TRANSLATION_LATENCY;
      VAPQ.push_back(pf_packet);
      return 1;
    }
    return 0;
  } else {
    auto success = add_pq(pf_packet);
    if (success)
      ++sim_stats.back().pf_issued;
    return success;
  }
}

int CACHE::prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  static bool deprecate_printed = false;
  if (!deprecate_printed) {
    std::cout << "WARNING: The extended signature CACHE::prefetch_line(ip, "
                 "base_addr, pf_addr, fill_this_level, prefetch_metadata) is "
                 "deprecated."
              << std::endl;
    std::cout << "WARNING: Use CACHE::prefetch_line(pf_addr, fill_this_level, "
                 "prefetch_metadata) instead."
              << std::endl;
    deprecate_printed = true;
  }
  return prefetch_line(pf_addr, fill_this_level, prefetch_metadata);
}

void CACHE::va_translate_prefetches()
{
  // TEMPORARY SOLUTION: mark prefetches as translated after a fixed latency
  if (!std::empty(VAPQ) && VAPQ.front().event_cycle <= current_cycle) {
    VAPQ.front().address = vmem.va_to_pa(cpu, VAPQ.front().v_address).first;

    // move the translated prefetch over to the regular PQ
    auto success = add_pq(VAPQ.front());
    if (success) {
      // remove the prefetch from the VAPQ
      VAPQ.pop_front();
      sim_stats.back().pf_issued++;
    }
  }
}

bool CACHE::add_pq(const PACKET& packet)
{
  assert(packet.address != 0);
  sim_stats.back().PQ_ACCESS++;

  DP(if (!warmup) {
    std::cout << "[" << NAME << "_WQ] " << __func__ << " instr_id: " << packet.instr_id << " address: " << std::hex << (packet.address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet.address << " v_address: " << packet.v_address << std::dec << " type: " << +packet.type << " occupancy: " << PQ.size();
  })

  // check occupancy
  if (std::size(PQ) >= PQ_SIZE) {

    DP(if (!warmup) std::cout << " FULL" << std::endl;)

    sim_stats.back().PQ_FULL++;
    return false; // cannot handle this request
  }

  // if there is no duplicate, add it to PQ
  PQ.push_back(packet);
  PQ.back().forward_checked = false;
  PQ.back().event_cycle = current_cycle + (warmup ? 0 : HIT_LATENCY);

  DP(if (!warmup) std::cout << " ADDED" << std::endl;)

  sim_stats.back().PQ_TO_CACHE++;
  return true;
}

void CACHE::return_data(const PACKET& packet)
{
  // check MSHR information
  auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(packet.address, OFFSET_BITS));
  auto first_unreturned = std::find_if(MSHR.begin(), MSHR.end(), [](auto x) { return x.event_cycle == std::numeric_limits<uint64_t>::max(); });

  // sanity check
  if (mshr_entry == MSHR.end()) {
    std::cerr << "[" << NAME << "_MSHR] " << __func__ << " instr_id: " << packet.instr_id << " cannot find a matching entry!";
    std::cerr << " address: " << std::hex << packet.address;
    std::cerr << " v_address: " << packet.v_address;
    std::cerr << " address: " << (packet.address >> OFFSET_BITS) << std::dec;
    std::cerr << " event: " << packet.event_cycle << " current: " << current_cycle << std::endl;
    assert(0);
  }

  // MSHR holds the most updated information about this request
  mshr_entry->data = packet.data;
  mshr_entry->pf_metadata = packet.pf_metadata;
  mshr_entry->event_cycle = current_cycle + (warmup ? 0 : FILL_LATENCY);

  DP(if (!warmup) {
    std::cout << "[" << NAME << "_MSHR] " << __func__ << " instr_id: " << mshr_entry->instr_id;
    std::cout << " address: " << std::hex << (mshr_entry->address >> OFFSET_BITS) << " full_addr: " << mshr_entry->address;
    std::cout << " data: " << mshr_entry->data << std::dec;
    std::cout << " index: " << std::distance(MSHR.begin(), mshr_entry) << " occupancy: " << get_occupancy(0, 0);
    std::cout << " event: " << mshr_entry->event_cycle << " current: " << current_cycle << std::endl;
  });

  // Order this entry after previously-returned entries, but before non-returned
  // entries
  std::iter_swap(mshr_entry, first_unreturned);
}

uint32_t CACHE::get_occupancy(uint8_t queue_type, uint64_t address)
{
  if (queue_type == 0)
    return std::count_if(MSHR.begin(), MSHR.end(), is_valid<PACKET>());
  else if (queue_type == 1)
    return RQ.size();
  else if (queue_type == 2)
    return WQ.size();
  else if (queue_type == 3)
    return PQ.size();

  return 0;
}

uint32_t CACHE::get_size(uint8_t queue_type, uint64_t address)
{
  if (queue_type == 0)
    return MSHR_SIZE;
  else if (queue_type == 1)
    return RQ_SIZE;
  else if (queue_type == 2)
    return WQ_SIZE;
  else if (queue_type == 3)
    return PQ_SIZE;

  return 0;
}

void CACHE::begin_phase()
{
    roi_stats.emplace_back();
    sim_stats.emplace_back();
}

void CACHE::end_phase(unsigned cpu)
{
    roi_stats.back().hits[cpu] = sim_stats.back().hits[cpu];
    roi_stats.back().misses[cpu] = sim_stats.back().misses[cpu];

    roi_stats.back().pf_requested = sim_stats.back().pf_requested;
    roi_stats.back().pf_issued = sim_stats.back().pf_issued;
    roi_stats.back().pf_useful = sim_stats.back().pf_useful;
    roi_stats.back().pf_useless = sim_stats.back().pf_useless;
    roi_stats.back().pf_fill = sim_stats.back().pf_fill;

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

    roi_stats.back().total_miss_latency = sim_stats.back().total_miss_latency;
}

void print_cache_stats(std::string name, uint32_t cpu, CACHE::stats_type stats)
{
    uint64_t TOTAL_HIT = std::accumulate(std::begin(stats.hits.at(cpu)), std::end(stats.hits[cpu]), 0ull),
             TOTAL_MISS = std::accumulate(std::begin(stats.hits.at(cpu)), std::end(stats.hits[cpu]), 0ull);

    std::cout << name << " TOTAL       ";
    std::cout << "ACCESS: " << std::setw(10) << TOTAL_HIT + TOTAL_MISS << "  ";
    std::cout << "HIT: "    << std::setw(10) << TOTAL_HIT << "  ";
    std::cout << "MISS: "   << std::setw(10) << TOTAL_MISS << std::endl;

    std::cout << name << " LOAD        ";
    std::cout << "ACCESS: " << std::setw(10) << stats.hits[cpu][LOAD] + stats.misses[cpu][LOAD] << "  ";
    std::cout << "HIT: "    << std::setw(10) << stats.hits[cpu][LOAD] << "  ";
    std::cout << "MISS: "   << std::setw(10) << stats.misses[cpu][LOAD] << std::endl;

    std::cout << name << " RFO         ";
    std::cout << "ACCESS: " << std::setw(10) << stats.hits[cpu][RFO] + stats.misses[cpu][RFO] << "  ";
    std::cout << "HIT: "    << std::setw(10) << stats.hits[cpu][RFO] << "  ";
    std::cout << "MISS: "   << std::setw(10) << stats.misses[cpu][RFO] << std::endl;

    std::cout << name << " PREFETCH    ";
    std::cout << "ACCESS: " << std::setw(10) << stats.hits[cpu][PREFETCH] + stats.misses[cpu][PREFETCH] << "  ";
    std::cout << "HIT: "    << std::setw(10) << stats.hits[cpu][PREFETCH] << "  ";
    std::cout << "MISS: "   << std::setw(10) << stats.misses[cpu][PREFETCH] << std::endl;

    std::cout << name << " WRITEBACK   ";
    std::cout << "ACCESS: " << std::setw(10) << stats.hits[cpu][WRITEBACK] + stats.misses[cpu][WRITEBACK] << "  ";
    std::cout << "HIT: "    << std::setw(10) << stats.hits[cpu][WRITEBACK] << "  ";
    std::cout << "MISS: "   << std::setw(10) << stats.misses[cpu][WRITEBACK] << std::endl;

    std::cout << name << " TRANSLATION ";
    std::cout << "ACCESS: " << std::setw(10) << stats.hits[cpu][TRANSLATION] + stats.misses[cpu][TRANSLATION] << "  ";
    std::cout << "HIT: "    << std::setw(10) << stats.hits[cpu][TRANSLATION] << "  ";
    std::cout << "MISS: "   << std::setw(10) << stats.misses[cpu][TRANSLATION] << std::endl;

    std::cout << name << " PREFETCH  ";
    std::cout << "REQUESTED: " << std::setw(10) << stats.pf_requested << "  ";
    std::cout << "ISSUED: " << std::setw(10) << stats.pf_issued << "  ";
    std::cout << "USEFUL: " << std::setw(10) << stats.pf_useful << "  ";
    std::cout << "USELESS: " << std::setw(10) << stats.pf_useless << std::endl;

    std::cout << name << " AVERAGE MISS LATENCY: " << (1.0*(stats.total_miss_latency))/TOTAL_MISS << " cycles" << std::endl;
    //std::cout << " AVERAGE MISS LATENCY: " << (stats.total_miss_latency)/TOTAL_MISS << " cycles " << stats.total_miss_latency << "/" << TOTAL_MISS<< std::endl;
}

void CACHE::print_roi_stats()
{
    for (std::size_t i = 0; i < NUM_CPUS; ++i)
        print_cache_stats(NAME, i, roi_stats.back());
}

void CACHE::print_phase_stats()
{
    for (std::size_t i = 0; i < NUM_CPUS; ++i)
        print_cache_stats(NAME, i, sim_stats.back());
}

bool CACHE::should_activate_prefetcher(int type) { return (1 << static_cast<int>(type)) & pref_activate_mask; }

void CACHE::print_deadlock()
{
  if (!std::empty(MSHR)) {
    std::cout << NAME << " MSHR Entry" << std::endl;
    std::size_t j = 0;
    for (PACKET entry : MSHR) {
      std::cout << "[" << NAME << " MSHR] entry: " << j++ << " instr_id: " << entry.instr_id;
      std::cout << " address: " << std::hex << (entry.address >> LOG2_BLOCK_SIZE) << " full_addr: " << entry.address << std::dec << " type: " << +entry.type;
      std::cout << " fill_level: " << +entry.fill_level << " event_cycle: " << entry.event_cycle << std::endl;
    }
  } else {
    std::cout << NAME << " MSHR empty" << std::endl;
  }
}
