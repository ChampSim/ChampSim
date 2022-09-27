#include "ptw.h"

#include "champsim.h"
#include "champsim_constants.h"
#include "instruction.h"
#include "util.h"
#include "vmem.h"

PageTableWalker::PageTableWalker(std::string v1, uint32_t cpu, double freq_scale, std::vector<champsim::simple_lru_table<uint64_t, uint64_t>>&& _pscl, uint32_t v10,
                                 uint32_t v11, uint32_t v12, uint32_t v13, uint64_t latency, MemoryRequestConsumer* ll, VirtualMemory& _vmem)
    : champsim::operable(freq_scale), MemoryRequestProducer(ll), NAME(v1), RQ_SIZE(v10), MSHR_SIZE(v11), MAX_READ(v12), MAX_FILL(v13),
      HIT_LATENCY(latency), pscl{_pscl}, vmem(_vmem), CR3_addr(_vmem.get_pte_pa(cpu, 0, std::size(pscl) + 1).first)
{
}

bool PageTableWalker::handle_read(const PACKET& handle_pkt)
{
  auto walk_base = CR3_addr;
  auto walk_init_level = std::size(pscl);
  for (auto cache = std::begin(pscl); cache != std::end(pscl); ++cache) {
    if (auto check_addr = cache->check_hit(handle_pkt.address); check_addr.has_value()) {
      walk_base = check_addr.value();
      walk_init_level = std::distance(cache, std::end(pscl)) - 1;
    }
  }
  auto walk_offset = vmem.get_offset(handle_pkt.address, walk_init_level + 1) * PTE_BYTES;

  if constexpr (champsim::debug_print) {
    std::cout << "[" << NAME << "] " << __func__ << " instr_id: " << handle_pkt.instr_id;
    std::cout << " address: " << std::hex << walk_base;
    std::cout << " v_address: " << handle_pkt.v_address << std::dec;
    std::cout << " pt_page offset: " << walk_offset / PTE_BYTES;
    std::cout << " translation_level: " << +walk_init_level << std::endl;
  }

  PACKET packet = handle_pkt;
  packet.v_address = handle_pkt.address;
  packet.init_translation_level = walk_init_level;
  packet.cycle_enqueued = current_cycle;

  return step_translation(splice_bits(walk_base, walk_offset, LOG2_PAGE_SIZE), walk_init_level, packet);
}

bool PageTableWalker::handle_fill(const PACKET& fill_mshr)
{
  if constexpr (champsim::debug_print) {
    std::cout << "[" << NAME << "] " << __func__ << " instr_id: " << fill_mshr.instr_id;
    std::cout << " address: " << std::hex << fill_mshr.address;
    std::cout << " v_address: " << fill_mshr.v_address;
    std::cout << " data: " << fill_mshr.data << std::dec;
    std::cout << " pt_page offset: " << ((fill_mshr.data & bitmask(LOG2_PAGE_SIZE)) >> lg2(PTE_BYTES));
    std::cout << " translation_level: " << +fill_mshr.translation_level;
    std::cout << " event: " << fill_mshr.event_cycle << " current: " << current_cycle << std::endl;
  }

  if (fill_mshr.translation_level == 0) {
    auto ret_pkt = fill_mshr;
    ret_pkt.address = fill_mshr.v_address;

    for (auto ret : ret_pkt.to_return)
      ret->return_data(ret_pkt);

    total_miss_latency += current_cycle - ret_pkt.cycle_enqueued;
    return true;
  } else {
    const auto pscl_idx = std::size(pscl) - fill_mshr.translation_level;
    pscl.at(pscl_idx).fill_cache(fill_mshr.v_address, fill_mshr.data);

    return step_translation(fill_mshr.data, fill_mshr.translation_level - 1, fill_mshr);
  }
}

bool PageTableWalker::step_translation(uint64_t addr, uint8_t transl_level, const PACKET& source)
{
  auto fwd_pkt = source;
  fwd_pkt.address = addr;
  fwd_pkt.type = TRANSLATION;
  fwd_pkt.to_return = {this};
  fwd_pkt.translation_level = transl_level;

  auto matches_and_inflight = [addr](const auto& x) {
    return (x.address >> LOG2_BLOCK_SIZE) == (addr >> LOG2_BLOCK_SIZE) && x.event_cycle == std::numeric_limits<uint64_t>::max();
  };
  auto mshr_entry = std::find_if(std::begin(MSHR), std::end(MSHR), matches_and_inflight);

  bool success = true;
  if (mshr_entry == std::end(MSHR))
    success = lower_level->add_rq(fwd_pkt);

  if (success) {
    fwd_pkt.to_return = source.to_return; // Set the return for MSHR packet same as read packet.
    fwd_pkt.type = source.type;
    fwd_pkt.event_cycle = std::numeric_limits<uint64_t>::max();
    MSHR.push_back(fwd_pkt);

    return true;
  }

  return false;
}

void PageTableWalker::operate()
{
  int fill_this_cycle = MAX_FILL;
  while (fill_this_cycle > 0 && !std::empty(MSHR) && MSHR.front().event_cycle <= current_cycle) {
    auto success = handle_fill(MSHR.front());
    if (!success)
      break;

    MSHR.pop_front();
    fill_this_cycle--;
  }

  int reads_this_cycle = MAX_READ;
  while (reads_this_cycle > 0 && !std::empty(RQ) && RQ.front().event_cycle <= current_cycle && std::size(MSHR) != MSHR_SIZE) {
    auto success = handle_read(RQ.front());
    if (!success)
      break;

    RQ.pop_front();
    reads_this_cycle--;
  }
}

bool PageTableWalker::add_rq(const PACKET& packet)
{
  assert(packet.address != 0);

  // check for duplicates in the read queue
  auto found_rq = std::find_if(RQ.begin(), RQ.end(), eq_addr(packet, LOG2_PAGE_SIZE));
  assert(found_rq == RQ.end()); // Duplicate request should not be sent.

  // check occupancy
  if (std::size(RQ) >= RQ_SIZE)
    return false; // cannot handle this request

  // if there is no duplicate, add it to RQ
  RQ.push_back(packet);
  RQ.back().event_cycle = current_cycle + (warmup ? 0 : HIT_LATENCY);

  return true;
}

void PageTableWalker::return_data(const PACKET& packet)
{
  for (auto& mshr_entry : MSHR) {
    if (eq_addr{packet, LOG2_BLOCK_SIZE}(mshr_entry)) {
      uint64_t penalty;
      if (mshr_entry.translation_level == 0)
        std::tie(mshr_entry.data, penalty) = vmem.va_to_pa(mshr_entry.asid, mshr_entry.v_address);
      else
        std::tie(mshr_entry.data, penalty) = vmem.get_pte_pa(mshr_entry.asid, mshr_entry.v_address, mshr_entry.translation_level);
      mshr_entry.event_cycle = current_cycle + (warmup ? 0 : penalty);

      if constexpr (champsim::debug_print) {
        std::cout << "[" << NAME << "_MSHR] " << __func__ << " instr_id: " << mshr_entry.instr_id;
        std::cout << " address: " << std::hex << mshr_entry.address;
        std::cout << " v_address: " << mshr_entry.v_address;
        std::cout << " data: " << mshr_entry.data << std::dec;
        std::cout << " translation_level: " << +mshr_entry.translation_level;
        std::cout << " occupancy: " << get_occupancy(0, mshr_entry.address);
        std::cout << " event: " << mshr_entry.event_cycle << " current: " << current_cycle << std::endl;
      }
    }
  }

  std::sort(std::begin(MSHR), std::end(MSHR), ord_event_cycle<PACKET>{});
}

uint32_t PageTableWalker::get_occupancy(uint8_t queue_type, uint64_t)
{
  if (queue_type == 0)
    return std::size(MSHR);
  else if (queue_type == 1)
    return std::size(RQ);
  return 0;
}

uint32_t PageTableWalker::get_size(uint8_t queue_type, uint64_t)
{
  if (queue_type == 0)
    return MSHR_SIZE;
  else if (queue_type == 1)
    return RQ_SIZE;
  return 0;
}

void PageTableWalker::print_deadlock()
{
  if (!std::empty(MSHR)) {
    std::cout << NAME << " MSHR Entry" << std::endl;
    std::size_t j = 0;
    for (PACKET entry : MSHR) {
      std::cout << "[" << NAME << " MSHR] entry: " << j++ << " instr_id: " << entry.instr_id;
      std::cout << " address: " << std::hex << entry.address << " v_address: " << entry.v_address << std::dec << " type: " << +entry.type;
      std::cout << " translation_level: " << +entry.translation_level << " event_cycle: " << entry.event_cycle << std::endl;
    }
  } else {
    std::cout << NAME << " MSHR empty" << std::endl;
  }
}
