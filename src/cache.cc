#include "cache.h"

#include <algorithm>
#include <iomanip>
#include <iterator>
#include <numeric>

#include "champsim.h"
#include "champsim_constants.h"
#include "instruction.h"
#include "util.h"
#include "vmem.h"

bool CACHE::handle_fill(PACKET& fill_mshr)
{
  cpu = fill_mshr.cpu;
  asid = fill_mshr.asid;

  // find victim
  uint32_t set = get_set(fill_mshr.address);

  auto set_begin = std::next(std::begin(block), set * NUM_WAY);
  auto set_end = std::next(set_begin, NUM_WAY);
  auto first_inv = std::find_if_not(set_begin, set_end, is_valid<BLOCK>());
  uint32_t way = std::distance(set_begin, first_inv);
  if (way == NUM_WAY)
    way = impl_replacement_find_victim(fill_mshr.cpu, fill_mshr.instr_id, set, &block.data()[set * NUM_WAY], fill_mshr.ip, fill_mshr.address, fill_mshr.type);

  bool success = filllike_miss(set, way, fill_mshr);

  sim_stats.back().total_miss_latency += current_cycle - fill_mshr.cycle_enqueued;

  if (success) {
    for (auto ret : fill_mshr.to_return)
      ret->return_data(fill_mshr);
  }

  return success;
}

bool CACHE::handle_writeback(PACKET& handle_pkt)
{
  cpu = handle_pkt.cpu;
  asid = handle_pkt.asid;

  // access cache
  uint32_t set = get_set(handle_pkt.address);
  uint32_t way = get_way(handle_pkt.asid, handle_pkt.address, set);

  BLOCK &fill_block = block[set*NUM_WAY + way];

  if (way < NUM_WAY) { // HIT
    impl_replacement_update_state(handle_pkt.cpu, set, way, fill_block.address, handle_pkt.ip, 0, handle_pkt.type, true);

    // COLLECT STATS
    sim_stats.back().hits[handle_pkt.cpu][handle_pkt.type]++;

    // mark dirty
    fill_block.dirty = 1;

    return true;
  } else { // MISS
    if (match_offset_bits) {
      return readlike_miss(handle_pkt);
    } else {
      // find victim
      auto set_begin = std::next(std::begin(block), set * NUM_WAY);
      auto set_end = std::next(set_begin, NUM_WAY);
      auto first_inv = std::find_if_not(set_begin, set_end, is_valid<BLOCK>());
      way = std::distance(set_begin, first_inv);
      if (way == NUM_WAY)
        way = impl_replacement_find_victim(handle_pkt.cpu, handle_pkt.instr_id, set, &block.data()[set * NUM_WAY], handle_pkt.ip, handle_pkt.address, handle_pkt.type);

      return filllike_miss(set, way, handle_pkt);
    }
  }
}

bool CACHE::handle_read(PACKET& handle_pkt)
{
  cpu = handle_pkt.cpu;
  asid = handle_pkt.asid;

  // A (hopefully temporary) hack to know whether to send the evicted paddr or  vaddr to the prefetcher
  ever_seen_data |= (handle_pkt.v_address != handle_pkt.ip);

  uint32_t set = get_set(handle_pkt.address);
  uint32_t way = get_way(handle_pkt.asid, handle_pkt.address, set);

  if (way < NUM_WAY) { // HIT
    readlike_hit(set, way, handle_pkt);
    return true;
  } else {
    return readlike_miss(handle_pkt);
  }
}

bool CACHE::handle_prefetch(PACKET& handle_pkt)
{
  cpu = handle_pkt.cpu;
  asid = handle_pkt.asid;

  uint32_t set = get_set(handle_pkt.address);
  uint32_t way = get_way(handle_pkt.asid, handle_pkt.address, set);

  if (way < NUM_WAY) { // HIT
    readlike_hit(set, way, handle_pkt);
    return true;
  } else {
    return readlike_miss(handle_pkt);
  }
}

void CACHE::readlike_hit(std::size_t set, std::size_t way, const PACKET& handle_pkt)
{
  if constexpr (champsim::debug_print) {
    std::cout << "[" << NAME << "] " << __func__;
    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << (handle_pkt.address >> OFFSET_BITS);
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address << std::dec;
    std::cout << " type: " << +handle_pkt.type;
    std::cout << " cycle: " << current_cycle << std::endl;
  }

  BLOCK& hit_block = block[set * NUM_WAY + way];

  // update prefetcher on load instruction
  if (should_activate_prefetcher(handle_pkt)) {
    uint64_t pf_base_addr = (virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);
    hit_block.pf_metadata = impl_prefetcher_cache_operate(pf_base_addr, handle_pkt.ip, 1, handle_pkt.type, handle_pkt.pf_metadata);
  }

  // update replacement policy
  impl_replacement_update_state(handle_pkt.cpu, set, way, hit_block.address, handle_pkt.ip, 0, handle_pkt.type, true);

  // COLLECT STATS
  sim_stats.back().hits[handle_pkt.cpu][handle_pkt.type]++;

  auto copy{handle_pkt};
  copy.data = hit_block.data;
  for (auto ret : copy.to_return)
    ret->return_data(copy);

  // update prefetch stats and reset prefetch bit
  if (hit_block.prefetch) {
    sim_stats.back().pf_useful++;
    hit_block.prefetch = 0;
  }
}

bool CACHE::readlike_miss(const PACKET& handle_pkt)
{
  if constexpr (champsim::debug_print) {
    std::cout << "[" << NAME << "] " << __func__;
    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << (handle_pkt.address >> OFFSET_BITS);
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address << std::dec;
    std::cout << " type: " << +handle_pkt.type;
    std::cout << " cycle: " << current_cycle << std::endl;
  }

  // check mshr
  auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr(handle_pkt, OFFSET_BITS));
  bool mshr_full = (MSHR.size() == MSHR_SIZE);

  if (mshr_entry != MSHR.end()) // miss already inflight
  {
    auto instr_copy = std::move(mshr_entry->instr_depend_on_me);
    auto ret_copy = std::move(mshr_entry->to_return);

    std::set_union(std::begin(instr_copy), std::end(instr_copy), std::begin(handle_pkt.instr_depend_on_me), std::end(handle_pkt.instr_depend_on_me),
                   std::back_inserter(mshr_entry->instr_depend_on_me), [](ooo_model_instr& x, ooo_model_instr& y) { return x.instr_id < y.instr_id; });
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
  } else {
    if (mshr_full)  // not enough MSHR resource
      return false; // TODO should we allow prefetches anyway if they will not
                    // be filled to this level?

    auto fwd_pkt = handle_pkt;

    if (fwd_pkt.type == WRITE)
      fwd_pkt.type = RFO;

    if (handle_pkt.fill_this_level)
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
    if (!std::empty(fwd_pkt.to_return)) {
      mshr_entry = MSHR.insert(std::end(MSHR), handle_pkt);
      mshr_entry->cycle_enqueued = current_cycle;
      mshr_entry->event_cycle = std::numeric_limits<uint64_t>::max();
    }
  }

  // update prefetcher on load instructions and prefetches from upper levels
  if (should_activate_prefetcher(handle_pkt)) {
    uint64_t pf_base_addr = (virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);
    mshr_entry->pf_metadata = impl_prefetcher_cache_operate(pf_base_addr, handle_pkt.ip, 0, handle_pkt.type, handle_pkt.pf_metadata);
  }

  return true;
}

bool CACHE::filllike_miss(std::size_t set, std::size_t way, const PACKET& handle_pkt)
{
  if constexpr (champsim::debug_print) {
    std::cout << "[" << NAME << "] " << __func__;
    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << (handle_pkt.address >> OFFSET_BITS);
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address << std::dec;
    std::cout << " type: " << +handle_pkt.type;
    std::cout << " cycle: " << current_cycle << std::endl;
  }

  bool bypass = (way == NUM_WAY);
  assert(handle_pkt.type != WRITE || !bypass);

  auto pkt_address = (virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);
  if (!bypass) {
    BLOCK& fill_block = block[set * NUM_WAY + way];
    if (fill_block.dirty) {
      PACKET writeback_packet;

      writeback_packet.cpu = handle_pkt.cpu;
      writeback_packet.address = fill_block.address;
      writeback_packet.data = fill_block.data;
      writeback_packet.instr_id = handle_pkt.instr_id;
      writeback_packet.ip = 0;
      writeback_packet.type = WRITE;
      writeback_packet.pf_metadata = fill_block.pf_metadata;

      auto success = lower_level->add_wq(writeback_packet);
      if (!success)
        return false;
    }

    auto evicting_address = (ever_seen_data ? fill_block.address : fill_block.v_address) & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);

    if (fill_block.prefetch)
      sim_stats.back().pf_useless++;

    if (handle_pkt.type == PREFETCH)
      sim_stats.back().pf_fill++;

    fill_block = {
      true, //valid
      handle_pkt.prefetch_from_this, //prefetch
      (handle_pkt.type == WRITE || (handle_pkt.type == RFO && handle_pkt.to_return.empty())), //dirty
      handle_pkt.asid, //asid
      handle_pkt.address, //address
      handle_pkt.v_address, //v_address
      handle_pkt.data, //data
      handle_pkt.ip, //ip
      handle_pkt.cpu, //cpu
      handle_pkt.instr_id, //instr_id
      handle_pkt.pf_metadata // pf_metadata
    };

    fill_block.pf_metadata = impl_prefetcher_cache_fill(pkt_address, set, way, handle_pkt.type == PREFETCH, evicting_address, handle_pkt.pf_metadata);
    impl_replacement_update_state(handle_pkt.cpu, set, way, handle_pkt.address, handle_pkt.ip, evicting_address, handle_pkt.type, false);
  } else {
    impl_prefetcher_cache_fill(pkt_address, set, way, handle_pkt.type == PREFETCH, 0, handle_pkt.pf_metadata); // FIXME ignored result
    impl_replacement_update_state(handle_pkt.cpu, set, way, handle_pkt.address, handle_pkt.ip, 0, handle_pkt.type, false);
  }

  // COLLECT STATS
  sim_stats.back().misses[handle_pkt.cpu][handle_pkt.type]++;

  return true;
}

void CACHE::operate()
{
  auto write_bw = MAX_WRITE;
  auto read_bw = MAX_READ;

  for (bool success = true; success && write_bw > 0 && !std::empty(MSHR) && MSHR.front().event_cycle <= current_cycle; --write_bw) {
    success = handle_fill(MSHR.front());
    if (success)
      MSHR.pop_front();
  }

  for (bool success = true; success && write_bw > 0 && !std::empty(queues.WQ) && queues.wq_has_ready(); --write_bw) {
    success = handle_writeback(queues.WQ.front());
    if (success)
      queues.WQ.pop_front();
  }

  for (bool success = true; success && read_bw > 0 && !std::empty(queues.RQ) && queues.rq_has_ready(); --read_bw) {
    success = handle_read(queues.RQ.front());
    if (success)
      queues.RQ.pop_front();
  }

  for (bool success = true; success && read_bw > 0 && !std::empty(queues.PQ) && queues.pq_has_ready(); --read_bw) {
    success = handle_prefetch(queues.PQ.front());
    if (success)
      queues.PQ.pop_front();
  }

  impl_prefetcher_cycle_operate();
}

uint32_t CACHE::get_set(uint64_t address) const { return ((address >> OFFSET_BITS) & bitmask(lg2(NUM_SET))); }

uint32_t CACHE::get_way(uint16_t asid, uint64_t address, uint32_t set) const
{
  auto begin = std::next(block.begin(), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);
  return std::distance(begin, std::find_if(begin, end, eq_addr<BLOCK>(asid, address, OFFSET_BITS)));
}

int CACHE::invalidate_entry(uint16_t asid, uint64_t inval_addr)
{
  uint32_t set = get_set(inval_addr);
  uint32_t way = get_way(asid, inval_addr, set);

  if (way < NUM_WAY)
    block[set * NUM_WAY + way].valid = 0;

  return way;
}

bool CACHE::add_rq(const PACKET& packet)
{
  if constexpr (champsim::debug_print) {
    std::cout << "[" << NAME << "_RQ] " << __func__ << " instr_id: " << packet.instr_id << " address: " << std::hex << (packet.address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet.address << " v_address: " << packet.v_address << std::dec << " type: " << +packet.type
              << " occupancy: " << std::size(queues.RQ) << " current_cycle: " << current_cycle;
  }

  return queues.add_rq(packet);
}

bool CACHE::add_wq(const PACKET& packet)
{
  if constexpr (champsim::debug_print) {
    std::cout << "[" << NAME << "_WQ] " << __func__ << " instr_id: " << packet.instr_id << " address: " << std::hex << (packet.address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet.address << " v_address: " << packet.v_address << std::dec << " type: " << +packet.type
              << " occupancy: " << std::size(queues.WQ) << " current_cycle: " << current_cycle;
  }

  return queues.add_wq(packet);
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
  pf_packet.asid = asid;

  auto success = queues.add_pq(pf_packet);
  if (success)
    ++sim_stats.back().pf_issued;
  return success;
}

int CACHE::prefetch_line(uint64_t, uint64_t, uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  return prefetch_line(pf_addr, fill_this_level, prefetch_metadata);
}

bool CACHE::add_pq(const PACKET& packet)
{
  if constexpr (champsim::debug_print) {
    std::cout << "[" << NAME << "_WQ] " << __func__ << " instr_id: " << packet.instr_id << " address: " << std::hex << (packet.address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet.address << " v_address: " << packet.v_address << std::dec << " type: " << +packet.type
              << " occupancy: " << std::size(queues.PQ) << " current_cycle: " << current_cycle;
  }

  return queues.add_pq(packet);
}

void CACHE::return_data(const PACKET& packet)
{
  // check MSHR information
  auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr(packet, OFFSET_BITS));
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

  // Order this entry after previously-returned entries, but before non-returned  entries
  std::iter_swap(mshr_entry, first_unreturned);
}

uint32_t CACHE::get_occupancy(uint8_t queue_type, uint64_t)
{
  if (queue_type == 0)
    return std::count_if(MSHR.begin(), MSHR.end(), is_valid<PACKET>());
  else if (queue_type == 1)
    return std::size(queues.RQ);
  else if (queue_type == 2)
    return std::size(queues.WQ);
  else if (queue_type == 3)
    return std::size(queues.PQ);

  return 0;
}

uint32_t CACHE::get_size(uint8_t queue_type, uint64_t)
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

  roi_stats.back().total_miss_latency = sim_stats.back().total_miss_latency;
}

void print_cache_stats(std::string name, uint32_t cpu, CACHE::stats_type stats)
{
  uint64_t TOTAL_HIT = std::accumulate(std::begin(stats.hits.at(cpu)), std::end(stats.hits[cpu]), 0ull),
           TOTAL_MISS = std::accumulate(std::begin(stats.hits.at(cpu)), std::end(stats.hits[cpu]), 0ull);

  std::cout << name << " TOTAL       ";
  std::cout << "ACCESS: " << std::setw(10) << TOTAL_HIT + TOTAL_MISS << "  ";
  std::cout << "HIT: " << std::setw(10) << TOTAL_HIT << "  ";
  std::cout << "MISS: " << std::setw(10) << TOTAL_MISS << std::endl;

  std::cout << name << " LOAD        ";
  std::cout << "ACCESS: " << std::setw(10) << stats.hits[cpu][LOAD] + stats.misses[cpu][LOAD] << "  ";
  std::cout << "HIT: " << std::setw(10) << stats.hits[cpu][LOAD] << "  ";
  std::cout << "MISS: " << std::setw(10) << stats.misses[cpu][LOAD] << std::endl;

  std::cout << name << " RFO         ";
  std::cout << "ACCESS: " << std::setw(10) << stats.hits[cpu][RFO] + stats.misses[cpu][RFO] << "  ";
  std::cout << "HIT: " << std::setw(10) << stats.hits[cpu][RFO] << "  ";
  std::cout << "MISS: " << std::setw(10) << stats.misses[cpu][RFO] << std::endl;

  std::cout << name << " PREFETCH    ";
  std::cout << "ACCESS: " << std::setw(10) << stats.hits[cpu][PREFETCH] + stats.misses[cpu][PREFETCH] << "  ";
  std::cout << "HIT: " << std::setw(10) << stats.hits[cpu][PREFETCH] << "  ";
  std::cout << "MISS: " << std::setw(10) << stats.misses[cpu][PREFETCH] << std::endl;

  std::cout << name << " WRITE       ";
  std::cout << "ACCESS: " << std::setw(10) << stats.hits[cpu][WRITE] + stats.misses[cpu][WRITE] << "  ";
  std::cout << "HIT: " << std::setw(10) << stats.hits[cpu][WRITE] << "  ";
  std::cout << "MISS: " << std::setw(10) << stats.misses[cpu][WRITE] << std::endl;

  std::cout << name << " TRANSLATION ";
  std::cout << "ACCESS: " << std::setw(10) << stats.hits[cpu][TRANSLATION] + stats.misses[cpu][TRANSLATION] << "  ";
  std::cout << "HIT: " << std::setw(10) << stats.hits[cpu][TRANSLATION] << "  ";
  std::cout << "MISS: " << std::setw(10) << stats.misses[cpu][TRANSLATION] << std::endl;

  std::cout << name << " PREFETCH  ";
  std::cout << "REQUESTED: " << std::setw(10) << stats.pf_requested << "  ";
  std::cout << "ISSUED: " << std::setw(10) << stats.pf_issued << "  ";
  std::cout << "USEFUL: " << std::setw(10) << stats.pf_useful << "  ";
  std::cout << "USELESS: " << std::setw(10) << stats.pf_useless << std::endl;

  std::cout << name << " AVERAGE MISS LATENCY: " << (1.0 * (stats.total_miss_latency)) / TOTAL_MISS << " cycles" << std::endl;
  // std::cout << " AVERAGE MISS LATENCY: " << (stats.total_miss_latency)/TOTAL_MISS << " cycles " << stats.total_miss_latency << "/" << TOTAL_MISS<< std::endl;
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

bool CACHE::should_activate_prefetcher(const PACKET& pkt) const { return (1 << static_cast<int>(pkt.type)) & pref_activate_mask; }

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
