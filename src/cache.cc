#include "cache.h"

#include <algorithm>
#include <iomanip>
#include <iterator>
#include <numeric>

#include "champsim.h"
#include "champsim_constants.h"
#include "instruction.h"
#include "util.h"

bool CACHE::handle_fill(const PACKET& fill_mshr)
{
  cpu = fill_mshr.cpu;

  // find victim
  auto set = get_set(fill_mshr.address);

  auto set_begin = std::next(std::begin(block), set * NUM_WAY);
  auto set_end = std::next(set_begin, NUM_WAY);
  auto first_inv = std::find_if_not(set_begin, set_end, is_valid<BLOCK>());
  auto way = std::distance(set_begin, first_inv);
  if (way == NUM_WAY)
    way = impl_replacement_find_victim(fill_mshr.cpu, fill_mshr.instr_id, set, &block.data()[set * NUM_WAY], fill_mshr.ip, fill_mshr.address, fill_mshr.type);

  if constexpr (champsim::debug_print) {
    std::cout << "[" << NAME << "] " << __func__;
    std::cout << " instr_id: " << fill_mshr.instr_id << " address: " << std::hex << (fill_mshr.address >> OFFSET_BITS);
    std::cout << " full_addr: " << fill_mshr.address;
    std::cout << " full_v_addr: " << fill_mshr.v_address << std::dec;
    std::cout << " set: " << set;
    std::cout << " way: " << way;
    std::cout << " type: " << +fill_mshr.type;
    std::cout << " cycle: " << current_cycle << std::endl;
  }

  const bool bypass = (way == NUM_WAY);
  assert(fill_mshr.type != WRITE || !bypass);

  bool success = true;

  auto metadata_thru = fill_mshr.pf_metadata;
  auto pkt_address = (virtual_prefetch ? fill_mshr.v_address : fill_mshr.address) & ~champsim::bitmask(match_offset_bits ? 0 : OFFSET_BITS);
  if (!bypass) {
    BLOCK& fill_block = block[set * NUM_WAY + way];
    if (fill_block.dirty) {
      PACKET writeback_packet;

      writeback_packet.cpu = fill_mshr.cpu;
      writeback_packet.address = fill_block.address;
      writeback_packet.data = fill_block.data;
      writeback_packet.instr_id = fill_mshr.instr_id;
      writeback_packet.ip = 0;
      writeback_packet.type = WRITE;
      writeback_packet.pf_metadata = fill_block.pf_metadata;

      success = lower_level->add_wq(writeback_packet);
    }

    if (success) {
      auto evicting_address = (ever_seen_data ? fill_block.address : fill_block.v_address) & ~champsim::bitmask(match_offset_bits ? 0 : OFFSET_BITS);

      if (fill_block.prefetch)
        sim_stats.back().pf_useless++;

      if (fill_mshr.type == PREFETCH)
        sim_stats.back().pf_fill++;

      fill_block.valid = true;
      fill_block.prefetch = fill_mshr.prefetch_from_this;
      fill_block.dirty = (fill_mshr.type == WRITE);
      fill_block.address = fill_mshr.address;
      fill_block.v_address = fill_mshr.v_address;
      fill_block.data = fill_mshr.data;
      fill_block.ip = fill_mshr.ip;
      fill_block.cpu = fill_mshr.cpu;
      fill_block.instr_id = fill_mshr.instr_id;

      metadata_thru = impl_prefetcher_cache_fill(pkt_address, set, way, fill_mshr.type == PREFETCH, evicting_address, metadata_thru);
      impl_replacement_update_state(fill_mshr.cpu, set, way, fill_mshr.address, fill_mshr.ip, evicting_address, fill_mshr.type, false);

      fill_block.pf_metadata = metadata_thru;
    }
  } else {
    metadata_thru = impl_prefetcher_cache_fill(pkt_address, set, way, fill_mshr.type == PREFETCH, 0, metadata_thru);
    impl_replacement_update_state(fill_mshr.cpu, set, way, fill_mshr.address, fill_mshr.ip, 0, fill_mshr.type, false);
  }

  if (success) {
    // COLLECT STATS
    sim_stats.back().total_miss_latency += current_cycle - fill_mshr.cycle_enqueued;

    auto copy{fill_mshr};
    copy.pf_metadata = metadata_thru;
    for (auto ret : copy.to_return)
      ret->return_data(copy);
  }

  return success;
}

bool CACHE::try_hit(const PACKET& handle_pkt)
{
  cpu = handle_pkt.cpu;

  // access cache
  auto set = get_set(handle_pkt.address);
  auto way = get_way(handle_pkt.address, set);
  const auto hit = (way < NUM_WAY);

  if constexpr (champsim::debug_print) {
    std::cout << "[" << NAME << "] " << __func__;
    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << (handle_pkt.address >> OFFSET_BITS);
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address << std::dec;
    std::cout << " set: " << set;
    std::cout << " way: " << way << " (" << ((way < NUM_WAY) ? "HIT" : "MISS") << ")";
    std::cout << " type: " << +handle_pkt.type;
    std::cout << " cycle: " << current_cycle << std::endl;
  }

  // update prefetcher on load instructions and prefetches from upper levels
  auto metadata_thru = handle_pkt.pf_metadata;
  if (should_activate_prefetcher(handle_pkt)) {
    uint64_t pf_base_addr = (virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~champsim::bitmask(match_offset_bits ? 0 : OFFSET_BITS);
    metadata_thru = impl_prefetcher_cache_operate(pf_base_addr, handle_pkt.ip, hit, handle_pkt.type, metadata_thru);
  }

  if (hit) {
    sim_stats.back().hits[handle_pkt.type][handle_pkt.cpu]++;

    BLOCK& hit_block = block[set * NUM_WAY + way];

    // update replacement policy
    impl_replacement_update_state(handle_pkt.cpu, set, way, hit_block.address, handle_pkt.ip, 0, handle_pkt.type, true);

    auto copy{handle_pkt};
    copy.data = hit_block.data;
    copy.pf_metadata = metadata_thru;
    for (auto ret : copy.to_return)
      ret->return_data(copy);

    hit_block.dirty = (handle_pkt.type == WRITE);

    // update prefetch stats and reset prefetch bit
    if (hit_block.prefetch) {
      sim_stats.back().pf_useful++;
      hit_block.prefetch = 0;
    }
  } else {
    sim_stats.back().misses[handle_pkt.type][handle_pkt.cpu]++;
  }

  return hit;
}

bool CACHE::handle_miss(const PACKET& handle_pkt)
{
  if constexpr (champsim::debug_print) {
    std::cout << "[" << NAME << "] " << __func__;
    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << (handle_pkt.address >> OFFSET_BITS);
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address << std::dec;
    std::cout << " type: " << +handle_pkt.type;
    std::cout << " local_prefetch: " << std::boolalpha << handle_pkt.prefetch_from_this << std::noboolalpha;
    std::cout << " cycle: " << current_cycle << std::endl;
  }

  cpu = handle_pkt.cpu;

  // check mshr
  auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(handle_pkt.address, OFFSET_BITS));
  bool mshr_full = (MSHR.size() == MSHR_SIZE);

  if (mshr_entry != MSHR.end()) // miss already inflight
  {
    auto instr_copy = std::move(mshr_entry->instr_depend_on_me);
    auto ret_copy = std::move(mshr_entry->to_return);

    std::set_union(std::begin(instr_copy), std::end(instr_copy), std::begin(handle_pkt.instr_depend_on_me), std::end(handle_pkt.instr_depend_on_me),
                   std::back_inserter(mshr_entry->instr_depend_on_me), ooo_model_instr::program_order);
    std::set_union(std::begin(ret_copy), std::end(ret_copy), std::begin(handle_pkt.to_return), std::end(handle_pkt.to_return),
                   std::back_inserter(mshr_entry->to_return));

    if (mshr_entry->type == PREFETCH && handle_pkt.type != PREFETCH) {
      // Mark the prefetch as useful
      if (mshr_entry->prefetch_from_this)
        sim_stats.back().pf_useful++;

      uint64_t prior_event_cycle = mshr_entry->event_cycle;
      auto to_return = std::move(mshr_entry->to_return);
      *mshr_entry = handle_pkt;

      // in case request is already returned, we should keep event_cycle
      mshr_entry->event_cycle = prior_event_cycle;
      mshr_entry->to_return = std::move(to_return);
    }

    if (should_activate_prefetcher(handle_pkt)) {
      uint64_t pf_base_addr = (virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~champsim::bitmask(match_offset_bits ? 0 : OFFSET_BITS);
      mshr_entry->pf_metadata = impl_prefetcher_cache_operate(pf_base_addr, handle_pkt.ip, 0, handle_pkt.type, handle_pkt.pf_metadata);
    }
  } else {
    if (mshr_full)  // not enough MSHR resource
      return false; // TODO should we allow prefetches anyway if they will not be filled to this level?

    auto fwd_pkt = handle_pkt;

    if (fwd_pkt.type == WRITE)
      fwd_pkt.type = RFO;

    if (handle_pkt.fill_this_level)
      fwd_pkt.to_return = {this};
    else
      fwd_pkt.to_return.clear();

    fwd_pkt.prefetch_from_this = false;

    bool success;
    if (prefetch_as_load || handle_pkt.type != PREFETCH)
      success = lower_level->add_rq(fwd_pkt);
    else
      success = lower_level->add_pq(fwd_pkt);

    if (!success)
      return false;

    // Allocate an MSHR
    if (!std::empty(fwd_pkt.to_return)) {
      mshr_entry = MSHR.insert(std::end(MSHR), handle_pkt);
      mshr_entry->pf_metadata = fwd_pkt.pf_metadata;
      mshr_entry->cycle_enqueued = current_cycle;
      mshr_entry->event_cycle = std::numeric_limits<uint64_t>::max();
    }
  }

  return true;
}

void CACHE::operate()
{
  auto tag_bw = MAX_TAG;
  auto fill_bw = MAX_FILL;

  auto do_fill = [&fill_bw, cycle=current_cycle, this](const PACKET& x){
    return x.event_cycle <= cycle && (fill_bw-- > 0) && this->handle_fill(x);
  };

  auto operate_readlike = [&,this](const PACKET& pkt) {
    return queues.is_ready(pkt) && (tag_bw-- > 0) && (this->try_hit(pkt) || this->handle_miss(pkt));
  };

  auto mshr_end = std::find_if_not(std::cbegin(MSHR), std::cend(MSHR), do_fill);
  MSHR.erase(std::cbegin(MSHR), mshr_end);

  auto write_end = std::find_if_not(std::cbegin(inflight_writes), std::cend(inflight_writes), do_fill);
  inflight_writes.erase(std::cbegin(inflight_writes), write_end);

  if (match_offset_bits) {
    // Treat writes (that is, stores) like reads
    auto wq_end = std::find_if_not(std::cbegin(queues.WQ), std::cend(queues.WQ), operate_readlike);
    queues.WQ.erase(std::begin(queues.WQ), wq_end);
  } else {
    // Treat writes (that is, writebacks) like fills
    auto wq_end = std::find_if_not(std::begin(queues.WQ), std::end(queues.WQ), [&](const PACKET& pkt) { return queues.is_ready(pkt) && (tag_bw-- > 0); });
    std::for_each(std::begin(queues.WQ), wq_end, [cycle=current_cycle+FILL_LATENCY](PACKET& pkt){ pkt.event_cycle = cycle; }); // apply fill latency
    std::remove_copy_if(std::begin(queues.WQ), wq_end, std::back_inserter(inflight_writes), [this](const PACKET& pkt){ return this->try_hit(pkt); }); // mark as inflight
    queues.WQ.erase(std::begin(queues.WQ), wq_end);
  }

  auto rq_end = std::find_if_not(std::cbegin(queues.RQ), std::cend(queues.RQ), operate_readlike);
  queues.RQ.erase(std::cbegin(queues.RQ), rq_end);

  auto pq_end = std::find_if_not(std::cbegin(queues.PQ), std::cend(queues.PQ), operate_readlike);
  queues.PQ.erase(std::cbegin(queues.PQ), pq_end);

  impl_prefetcher_cycle_operate();
}

uint64_t CACHE::get_set(uint64_t address) const { return (address >> OFFSET_BITS) & champsim::bitmask(champsim::lg2(NUM_SET)); }

uint64_t CACHE::get_way(uint64_t address, uint64_t set) const
{
  auto begin = std::next(block.begin(), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);
  return std::distance(begin, std::find_if(begin, end, eq_addr<BLOCK>(address, OFFSET_BITS)));
}

uint64_t CACHE::invalidate_entry(uint64_t inval_addr)
{
  auto set = get_set(inval_addr);
  auto way = get_way(inval_addr, set);

  if (way < NUM_WAY)
    block[set * NUM_WAY + way].valid = 0;

  return way;
}

bool CACHE::add_rq(const PACKET& packet)
{
  if constexpr (champsim::debug_print) {
    std::cout << "[" << NAME << "_RQ] " << __func__ << " instr_id: " << packet.instr_id << " address: " << std::hex << (packet.address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet.address << " v_address: " << packet.v_address << std::dec << " type: " << +packet.type
              << " occupancy: " << std::size(queues.RQ) << " current_cycle: " << current_cycle << std::endl;
  }

  return queues.add_rq(packet);
}

bool CACHE::add_wq(const PACKET& packet)
{
  if constexpr (champsim::debug_print) {
    std::cout << "[" << NAME << "_WQ] " << __func__ << " instr_id: " << packet.instr_id << " address: " << std::hex << (packet.address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet.address << " v_address: " << packet.v_address << std::dec << " type: " << +packet.type
              << " occupancy: " << std::size(queues.WQ) << " current_cycle: " << current_cycle << std::endl;
  }

  return queues.add_wq(packet);
}

bool CACHE::add_pq(const PACKET& packet)
{
  if constexpr (champsim::debug_print) {
    std::cout << "[" << NAME << "_PQ] " << __func__ << " instr_id: " << packet.instr_id << " address: " << std::hex << (packet.address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet.address << " v_address: " << packet.v_address << std::dec << " type: " << +packet.type
              << " occupancy: " << std::size(queues.PQ) << " current_cycle: " << current_cycle << std::endl;
  }

  return queues.add_pq(packet);
}

int CACHE::prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  sim_stats.back().pf_requested++;

  PACKET pf_packet;
  pf_packet.type = PREFETCH;
  pf_packet.prefetch_from_this = true;
  pf_packet.fill_this_level = fill_this_level;
  pf_packet.pf_metadata = prefetch_metadata;
  pf_packet.cpu = cpu;
  pf_packet.address = pf_addr;
  pf_packet.v_address = virtual_prefetch ? pf_addr : 0;

  auto success = queues.add_pq(pf_packet);
  if (success)
    ++sim_stats.back().pf_issued;
  return success;
}

int CACHE::prefetch_line(uint64_t, uint64_t, uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  return prefetch_line(pf_addr, fill_this_level, prefetch_metadata);
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

  if constexpr (champsim::debug_print) {
    std::cout << "[" << NAME << "_MSHR] " << __func__ << " instr_id: " << mshr_entry->instr_id;
    std::cout << " address: " << std::hex << mshr_entry->address;
    std::cout << " data: " << mshr_entry->data << std::dec;
    std::cout << " event: " << mshr_entry->event_cycle << " current: " << current_cycle << std::endl;
  }

  // Order this entry after previously-returned entries, but before non-returned
  // entries
  std::iter_swap(mshr_entry, first_unreturned);
}

std::size_t CACHE::get_occupancy(uint8_t queue_type, uint64_t)
{
  if (queue_type == 0)
    return std::size(MSHR);
  else if (queue_type == 1)
    return std::size(queues.RQ);
  else if (queue_type == 2)
    return std::size(queues.WQ);
  else if (queue_type == 3)
    return std::size(queues.PQ);

  return 0;
}

std::size_t CACHE::get_size(uint8_t queue_type, uint64_t)
{
  if (queue_type == 0)
    return MSHR_SIZE;
  else if (queue_type == 1)
    return queues.RQ_SIZE;
  else if (queue_type == 2)
    return queues.WQ_SIZE;
  else if (queue_type == 3)
    return queues.PQ_SIZE;

  return 0;
}

void CACHE::initialize()
{
  impl_prefetcher_initialize();
  impl_replacement_initialize();
}

void CACHE::begin_phase()
{
  roi_stats.emplace_back();
  sim_stats.emplace_back();

  roi_stats.back().name = NAME;
  sim_stats.back().name = NAME;
}

void CACHE::end_phase(unsigned finished_cpu)
{
  for (auto type : {LOAD, RFO, PREFETCH, WRITE, TRANSLATION}) {
    roi_stats.back().hits.at(type).at(finished_cpu) = sim_stats.back().hits.at(type).at(finished_cpu);
    roi_stats.back().misses.at(type).at(finished_cpu) = sim_stats.back().misses.at(type).at(finished_cpu);
  }

  roi_stats.back().pf_requested = sim_stats.back().pf_requested;
  roi_stats.back().pf_issued = sim_stats.back().pf_issued;
  roi_stats.back().pf_useful = sim_stats.back().pf_useful;
  roi_stats.back().pf_useless = sim_stats.back().pf_useless;
  roi_stats.back().pf_fill = sim_stats.back().pf_fill;

  roi_stats.back().total_miss_latency = sim_stats.back().total_miss_latency;
}

bool CACHE::should_activate_prefetcher(const PACKET& pkt) const { return ((1 << pkt.type) & pref_activate_mask) && !pkt.prefetch_from_this; }

void CACHE::print_deadlock()
{
  if (!std::empty(MSHR)) {
    std::cout << NAME << " MSHR Entry" << std::endl;
    std::size_t j = 0;
    for (PACKET entry : MSHR) {
      std::cout << "[" << NAME << " MSHR] entry: " << j++ << " instr_id: " << entry.instr_id;
      std::cout << " address: " << std::hex << entry.address << " v_addr: " << entry.v_address << std::dec << " type: " << +entry.type;
      std::cout << " event_cycle: " << entry.event_cycle << std::endl;
    }
  } else {
    std::cout << NAME << " MSHR empty" << std::endl;
  }

  if (!std::empty(queues.RQ)) {
    for (const auto& entry : queues.RQ) {
      std::cout << "[" << NAME << " RQ] "
                << " instr_id: " << entry.instr_id;
      std::cout << " address: " << std::hex << entry.address << " v_addr: " << entry.v_address << std::dec << " type: " << +entry.type;
      std::cout << " event_cycle: " << entry.event_cycle << std::endl;
    }
  } else {
    std::cout << NAME << " RQ empty" << std::endl;
  }

  if (!std::empty(queues.WQ)) {
    for (const auto& entry : queues.WQ) {
      std::cout << "[" << NAME << " WQ] "
                << " instr_id: " << entry.instr_id;
      std::cout << " address: " << std::hex << entry.address << " v_addr: " << entry.v_address << std::dec << " type: " << +entry.type;
      std::cout << " event_cycle: " << entry.event_cycle << std::endl;
    }
  } else {
    std::cout << NAME << " WQ empty" << std::endl;
  }

  if (!std::empty(queues.PQ)) {
    for (const auto& entry : queues.PQ) {
      std::cout << "[" << NAME << " PQ] "
                << " instr_id: " << entry.instr_id;
      std::cout << " address: " << std::hex << entry.address << " v_addr: " << entry.v_address << std::dec << " type: " << +entry.type;
      std::cout << " event_cycle: " << entry.event_cycle << std::endl;
    }
  } else {
    std::cout << NAME << " PQ empty" << std::endl;
  }
}
