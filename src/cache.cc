#include "cache.h"

#include <algorithm>

#include "champsim.h"
#include "champsim_constants.h"
#include "util.h"
#include "vmem.h"

#ifndef SANITY_CHECK
#define NDEBUG
#endif

extern VirtualMemory vmem;
extern uint8_t warmup_complete[NUM_CPUS];

void CACHE::handle_fill()
{
  while (writes_available_this_cycle > 0) {
    auto fill_mshr = MSHR.begin();
    if (fill_mshr == std::end(MSHR) || fill_mshr->event_cycle > current_cycle)
      return;

    // find victim
    auto [set_begin, set_end] = get_set_span(fill_mshr->address);
    auto fill_block = check_block_by(set_begin, set_end, std::not_fn(is_valid<BLOCK>()));
    if (!fill_block.has_value()) {
      auto way = impl_replacement_find_victim(fill_mshr->cpu, fill_mshr->instr_id, get_set(fill_mshr->address), &(*set_begin), fill_mshr->ip,
                                              fill_mshr->address, fill_mshr->type);
      if (way != NUM_WAY)
        fill_block = std::next(set_begin, way);
    }

    bool success = filllike_miss(fill_block, *fill_mshr);
    if (!success)
      return;

    if (fill_block.has_value()) {
      // update processed packets
      fill_mshr->data = (*fill_block)->data;

      for (auto ret : fill_mshr->to_return)
        ret->return_data(*fill_mshr);
    }

    MSHR.erase(fill_mshr);
    writes_available_this_cycle--;
  }
}

void CACHE::handle_writeback()
{
  for (auto it = std::find_if(std::begin(WQ), std::end(WQ), std::not_fn(&PACKET::forward_checked)); it != std::end(WQ);) {
    if (auto found = std::find_if(std::begin(WQ), std::end(WQ), eq_addr<PACKET>(it->address, match_offset_bits ? 0 : OFFSET_BITS)); found != std::end(WQ)) {
      ++WQ_MERGED;
      it = WQ.erase(it);
    } else {
      it->forward_checked = true;
      ++it;
    }
  }

  while (!std::empty(WQ) && WQ.front().event_cycle < current_cycle && writes_available_this_cycle > 0) {
    // handle the oldest entry
    PACKET& handle_pkt = WQ.front();

    // access cache
    if (auto fill_block = check_hit(handle_pkt.address); fill_block.has_value()) // HIT
    {
      impl_replacement_update_state(handle_pkt.cpu, get_set(handle_pkt.address), get_way(handle_pkt.address), (*fill_block)->address, handle_pkt.ip, 0, handle_pkt.type, 1);

      // COLLECT STATS
      sim_hit[handle_pkt.cpu][handle_pkt.type]++;
      sim_access[handle_pkt.cpu][handle_pkt.type]++;

      // mark dirty
      (*fill_block)->dirty = 1;
    } else // MISS
    {
      bool success;
      if (handle_pkt.type == RFO && handle_pkt.to_return.empty()) {
        success = readlike_miss(handle_pkt);
      } else {
        // find victim

        auto [set_begin, set_end] = get_set_span(handle_pkt.address);
        auto fill_block = check_block_by(set_begin, set_end, std::not_fn(is_valid<BLOCK>()));
        if (!fill_block.has_value()) {
          auto way = impl_replacement_find_victim(handle_pkt.cpu, handle_pkt.instr_id, get_set(handle_pkt.address), &(*set_begin), handle_pkt.ip,
                                                  handle_pkt.address, handle_pkt.type);
          if (way != NUM_WAY)
            fill_block = std::next(set_begin, way);
        }

        success = filllike_miss(fill_block, handle_pkt);
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
  for (auto it = std::find_if(std::begin(RQ), std::end(RQ), std::not_fn(&PACKET::forward_checked)); it != std::end(RQ);) {
    if (auto found = std::find_if(std::begin(WQ), std::end(WQ), eq_addr<PACKET>(it->address, match_offset_bits ? 0 : OFFSET_BITS)); found != std::end(WQ)) {
      // A writeback was found in the WQ. Forward its data and return.
      ++WQ_FORWARD;
      it->data = found->data;
      for (auto ret : it->to_return)
          ret->return_data(*it);

      it = RQ.erase(it);
    } else if (auto found = std::find_if(std::begin(RQ), it, eq_addr<PACKET>(it->address, OFFSET_BITS)); found != it) {
      ++RQ_MERGED;
      found->fill_level = std::min(found->fill_level, it->fill_level);
      packet_dep_merge(found->lq_index_depend_on_me, it->lq_index_depend_on_me);
      packet_dep_merge(found->sq_index_depend_on_me, it->sq_index_depend_on_me);
      packet_dep_merge(found->instr_depend_on_me, it->instr_depend_on_me);
      packet_dep_merge(found->to_return, it->to_return);

      it = RQ.erase(it);
    } else {
      it->forward_checked = true;
      ++it;
    }
  }

  while (!std::empty(RQ) && RQ.front().event_cycle < current_cycle && reads_available_this_cycle > 0) {
    // handle the oldest entry
    PACKET& handle_pkt = RQ.front();

    // A (hopefully temporary) hack to know whether to send the evicted paddr or
    // vaddr to the prefetcher
    ever_seen_data |= (handle_pkt.v_address != handle_pkt.ip);

    if (auto hit_block = check_hit(handle_pkt.address); hit_block.has_value()) // HIT
    {
      readlike_hit(**hit_block, handle_pkt);
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
  for (auto it = std::find_if(std::begin(PQ), std::end(PQ), std::not_fn(&PACKET::forward_checked)); it != std::end(PQ);) {
    if (auto found = std::find_if(std::begin(WQ), std::end(WQ), eq_addr<PACKET>(it->address, match_offset_bits ? 0 : OFFSET_BITS)); found != std::end(WQ)) {
      // A writeback was found in the WQ. Forward its data and return.
      ++WQ_FORWARD;
      it->data = found->data;
      for (auto ret : it->to_return)
        ret->return_data(*it);

      it = PQ.erase(it);
    } else if (auto found = std::find_if(std::begin(PQ), it, eq_addr<PACKET>(it->address, OFFSET_BITS)); found != it) {
      ++PQ_MERGED;
      found->fill_level = std::min(found->fill_level, it->fill_level);
      packet_dep_merge(found->to_return, it->to_return);

      it = PQ.erase(it);
    } else {
      it->forward_checked = true;
      ++it;
    }
  }

  while (!std::empty(PQ) && PQ.front().event_cycle < current_cycle && reads_available_this_cycle > 0) {
    PACKET& handle_pkt = PQ.front();

    if (handle_pkt.v_address == handle_pkt.address) // not translated
    {
      auto [addr, fault] = vmem.va_to_pa(cpu, handle_pkt.v_address);
      handle_pkt.address = addr;
      handle_pkt.event_cycle = current_cycle + HIT_LATENCY + (fault ? vmem.minor_fault_penalty : 0);
      auto succ = std::upper_bound(std::begin(PQ), std::end(PQ), handle_pkt, cmp_event_cycle<PACKET>{});
      std::rotate(std::begin(PQ), std::next(std::begin(PQ)), succ);
    } else {
      if (auto hit_block = check_hit(handle_pkt.address); hit_block.has_value()) {
        readlike_hit(**hit_block, handle_pkt);
      } else {
        bool success = readlike_miss(handle_pkt);
        if (!success)
          return;
      }

      PQ.pop_front();
      reads_available_this_cycle--;
    }
  }
}

void CACHE::readlike_hit(BLOCK& hit_block, PACKET& handle_pkt)
{
  DP(if (warmup_complete[handle_pkt.cpu]) {
    std::cout << "[" << NAME << "] " << __func__ << " hit";
    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << (handle_pkt.address >> OFFSET_BITS);
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address << std::dec;
    std::cout << " type: " << +handle_pkt.type;
    std::cout << " cycle: " << current_cycle << std::endl;
  });

  handle_pkt.data = hit_block.data;

  // update prefetcher on load instruction
  if (should_activate_prefetcher(handle_pkt.type) && handle_pkt.pf_origin_level < fill_level) {
    cpu = handle_pkt.cpu;
    uint64_t pf_base_addr = (virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);
    handle_pkt.pf_metadata = impl_prefetcher_cache_operate(pf_base_addr, handle_pkt.ip, 1, handle_pkt.type, handle_pkt.pf_metadata);
  }

  // update replacement policy
  impl_replacement_update_state(handle_pkt.cpu, get_set(handle_pkt.address), get_way(handle_pkt.address), hit_block.address, handle_pkt.ip, 0, handle_pkt.type, 1);

  // COLLECT STATS
  sim_hit[handle_pkt.cpu][handle_pkt.type]++;
  sim_access[handle_pkt.cpu][handle_pkt.type]++;

  for (auto ret : handle_pkt.to_return)
    ret->return_data(handle_pkt);

  // update prefetch stats and reset prefetch bit
  if (hit_block.prefetch) {
    pf_useful++;
    hit_block.prefetch = 0;
  }
}

bool CACHE::readlike_miss(PACKET& handle_pkt)
{
  DP(if (warmup_complete[handle_pkt.cpu]) {
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
        pf_useful++;

      uint64_t prior_event_cycle = mshr_entry->event_cycle;
      *mshr_entry = handle_pkt;

      // in case request is already returned, we should keep event_cycle
      mshr_entry->event_cycle = prior_event_cycle;
    }
  } else {
    if (mshr_full)  // not enough MSHR resource
      return false; // TODO should we allow prefetches anyway if they will not
                    // be filled to this level?

    auto fwd_pkt = handle_pkt; // the packet to send to the lower level

    if (fwd_pkt.fill_level <= fill_level)
      fwd_pkt.to_return = {this};
    else
      fwd_pkt.to_return.clear();

    bool success;
    if (prefetch_as_load || (fwd_pkt.type != PREFETCH))
      success = lower_level->add_rq(fwd_pkt);
    else
      success = lower_level->add_pq(fwd_pkt);

    // Allocate an MSHR
    if (success && handle_pkt.fill_level <= fill_level) {
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

bool CACHE::filllike_miss(std::optional<typename decltype(block)::iterator> fill_block, PACKET& handle_pkt)
{
  DP(if (warmup_complete[handle_pkt.cpu]) {
    std::cout << "[" << NAME << "] " << __func__ << " miss";
    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << (handle_pkt.address >> OFFSET_BITS);
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address << std::dec;
    std::cout << " type: " << +handle_pkt.type;
    std::cout << " cycle: " << current_cycle << std::endl;
  });

  uint64_t evicting_address = 0;

  if (fill_block.has_value()) {
    assert(handle_pkt.type != WRITEBACK);

    if ((*fill_block)->dirty) {
      PACKET writeback_packet;

      writeback_packet.fill_level = lower_level->fill_level;
      writeback_packet.cpu = handle_pkt.cpu;
      writeback_packet.address = (*fill_block)->address;
      writeback_packet.data = (*fill_block)->data;
      writeback_packet.instr_id = handle_pkt.instr_id;
      writeback_packet.ip = 0;
      writeback_packet.type = WRITEBACK;

      auto success = lower_level->add_wq(writeback_packet);
      if (!success)
        return false;
    }

    if (ever_seen_data)
      evicting_address = (*fill_block)->address & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);
    else
      evicting_address = (*fill_block)->v_address & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);

    if ((*fill_block)->prefetch)
      pf_useless++;

    if (handle_pkt.type == PREFETCH)
      pf_fill++;

    **fill_block = {
        true,                                                                                       // valid
        (handle_pkt.type == PREFETCH && handle_pkt.pf_origin_level == fill_level),                  // prefetch
        (handle_pkt.type == WRITEBACK || (handle_pkt.type == RFO && handle_pkt.to_return.empty())), // dirty
        handle_pkt.address,                                                                         // address
        handle_pkt.v_address,                                                                       // v_address
        handle_pkt.data,                                                                            // data
        handle_pkt.ip,                                                                              // ip
        handle_pkt.cpu,                                                                             // cpu
        handle_pkt.instr_id,                                                                        // instr_id
        (*fill_block)->lru                                                                          // lru
    };
  }

  if (warmup_complete[handle_pkt.cpu] && (handle_pkt.cycle_enqueued != 0))
    total_miss_latency += current_cycle - handle_pkt.cycle_enqueued;

  // update prefetcher
  cpu = handle_pkt.cpu;
  handle_pkt.pf_metadata = impl_prefetcher_cache_fill(
      (virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS), get_set(handle_pkt.address),
      get_way(handle_pkt.address), handle_pkt.type == PREFETCH, evicting_address, handle_pkt.pf_metadata);

  // update replacement policy
  impl_replacement_update_state(handle_pkt.cpu, get_set(handle_pkt.address), get_way(handle_pkt.address), handle_pkt.address, handle_pkt.ip, 0, handle_pkt.type, 0);

  if (fill_block.has_value())
    (*fill_block)->lru = 0;

  // COLLECT STATS
  sim_miss[handle_pkt.cpu][handle_pkt.type]++;
  sim_access[handle_pkt.cpu][handle_pkt.type]++;

  return true;
}

void CACHE::operate()
{
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
  handle_prefetch();
}

uint32_t CACHE::get_set(uint64_t address) const { return ((address >> OFFSET_BITS) & bitmask(lg2(NUM_SET))); }

uint32_t CACHE::get_way(uint64_t address)
{
  auto [begin, end] = get_set_span(address);
  return std::distance(begin, check_hit(address).value_or(end));
}

auto CACHE::get_set_span(uint64_t address) -> std::pair<block_iter_t, block_iter_t>
{
  auto begin = std::next(std::begin(block), NUM_WAY * get_set(address));
  return {begin, std::next(begin, NUM_WAY)};
}

template <typename F>
auto CACHE::check_block_by(block_iter_t begin, block_iter_t end, F&& f) const -> std::optional<block_iter_t>
{
  auto found = std::find_if(begin, end, std::forward<F>(f));
  if (found == end)
    return {};
  return found;
}

auto CACHE::check_hit(uint64_t address) -> std::optional<block_iter_t>
{
  auto [begin, end] = get_set_span(address);
  return check_block_by(begin, end, eq_addr<BLOCK>(address, OFFSET_BITS));
}

bool CACHE::invalidate_entry(uint64_t inval_addr)
{
  auto hit_block = check_hit(inval_addr);
  if (hit_block.has_value())
    (*hit_block)->valid = 0;

  return hit_block.has_value();
}

bool CACHE::add_rq(PACKET packet)
{
  assert(packet.address != 0);
  RQ_ACCESS++;

  DP(if (warmup_complete[packet.cpu]) {
    std::cout << "[" << NAME << "_RQ] " << __func__ << " instr_id: " << packet.instr_id << " address: " << std::hex << (packet.address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet.address << " v_address: " << packet.v_address << std::dec << " type: " << +packet.type << " occupancy: " << RQ.size();
  })

  // check occupancy
  if (std::size(RQ) >= RQ_SIZE) {
    RQ_FULL++;

    DP(if (warmup_complete[packet.cpu]) std::cout << " FULL" << std::endl;)

    return false; // cannot handle this request
  }

  packet.event_cycle = current_cycle + warmup_complete[cpu] ? HIT_LATENCY : 0;
  packet.forward_checked = false;
  // if there is no duplicate, add it to RQ
  RQ.push_back(packet);

  DP(if (warmup_complete[packet.cpu]) std::cout << " ADDED" << std::endl;)

  RQ_TO_CACHE++;
  return true;
}

bool CACHE::add_wq(PACKET packet)
{
  WQ_ACCESS++;

  DP(if (warmup_complete[packet.cpu]) {
    std::cout << "[" << NAME << "_WQ] " << __func__ << " instr_id: " << packet.instr_id << " address: " << std::hex << (packet.address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet.address << " v_address: " << packet.v_address << std::dec << " type: " << +packet.type << " occupancy: " << WQ.size();
  })

  // Check for room in the queue
  if (std::size(WQ) >= WQ_SIZE) {
    DP(if (warmup_complete[packet.cpu]) std::cout << " FULL" << std::endl;)

    ++WQ_FULL;
    return false;
  }

  packet.event_cycle = current_cycle + warmup_complete[cpu] ? HIT_LATENCY : 0;
  packet.forward_checked = false;
  // if there is no duplicate, add it to the write queue
  WQ.push_back(packet);

  DP(if (warmup_complete[packet.cpu]) std::cout << " ADDED" << std::endl;)

  WQ_TO_CACHE++;
  WQ_ACCESS++;

  return true;
}

int CACHE::prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  pf_requested++;

  PACKET pf_packet;
  pf_packet.fill_level = (fill_this_level ? fill_level : lower_level->fill_level);
  pf_packet.pf_origin_level = fill_level;
  pf_packet.pf_metadata = prefetch_metadata;
  pf_packet.cpu = cpu;
  pf_packet.v_address = virtual_prefetch ? pf_addr : 0;
  pf_packet.address = pf_addr;
  pf_packet.type = PREFETCH;

  auto success = add_pq(pf_packet);
  if (!success)
    return 0;

  pf_issued++;
  return 1;
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

bool CACHE::add_pq(PACKET packet)
{
  assert(packet.address != 0);
  PQ_ACCESS++;

  DP(if (warmup_complete[packet.cpu]) {
    std::cout << "[" << NAME << "_WQ] " << __func__ << " instr_id: " << packet.instr_id << " address: " << std::hex << (packet.address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet.address << " v_address: " << packet.v_address << std::dec << " type: " << +packet.type
              << " occupancy: " << get_occupancy(3, packet.address);
  })

  // check occupancy
  if (std::size(PQ) == PQ_SIZE) {
    DP(if (warmup_complete[packet.cpu]) std::cout << " FULL" << std::endl;)

    PQ_FULL++;
    return false;
  }

  // if there is no duplicate, add it to PQ
  packet.forward_checked = false;
  if (packet.pf_origin_level == fill_level)
    packet.event_cycle = current_cycle + VA_PREFETCH_TRANSLATION_LATENCY;
  else
    packet.event_cycle = current_cycle + warmup_complete[cpu] ? HIT_LATENCY : 0;

  PQ.push_back(packet);

  DP(if (warmup_complete[packet.cpu]) std::cout << " ADDED" << std::endl;)

  PQ_TO_CACHE++;
  return true;
}

void CACHE::return_data(PACKET packet)
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
  mshr_entry->event_cycle = current_cycle + (warmup_complete[cpu] ? FILL_LATENCY : 0);

  DP(if (warmup_complete[packet.cpu]) {
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
