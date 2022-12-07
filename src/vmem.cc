#include "vmem.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>

#include "champsim.h"
#include "champsim_constants.h"
#include "dram_controller.h"
#include "util/bits.h"

VirtualMemory::VirtualMemory(unsigned paddr_bits, uint64_t page_table_page_size, std::size_t page_table_levels, uint64_t minor_penalty, MEMORY_CONTROLLER& dram)
    : next_ppage(VMEM_RESERVE_CAPACITY), last_ppage(1ull << paddr_bits), minor_fault_penalty(minor_penalty), pt_levels(page_table_levels),
      pte_page_size(page_table_page_size)
{
  assert(page_table_page_size > 1024);
  assert(page_table_page_size == (1ull << champsim::lg2(page_table_page_size)));
  assert(VMEM_RESERVE_CAPACITY >= PAGE_SIZE);
  assert(last_ppage > VMEM_RESERVE_CAPACITY);

  if (paddr_bits > champsim::lg2(dram.size()))
    std::cout << "WARNING: physical memory size is smaller than virtual memory size" << std::endl;
}

uint64_t VirtualMemory::shamt(std::size_t level) const { return LOG2_PAGE_SIZE + champsim::lg2(pte_page_size / PTE_BYTES) * (level - 1); }

uint64_t VirtualMemory::get_offset(uint64_t vaddr, std::size_t level) const
{
  return (vaddr >> shamt(level)) & champsim::bitmask(champsim::lg2(pte_page_size / PTE_BYTES));
}

uint64_t VirtualMemory::ppage_front() const
{
  assert(available_ppages() > 0);
  return next_ppage;
}

void VirtualMemory::ppage_pop() { next_ppage += PAGE_SIZE; }

std::size_t VirtualMemory::available_ppages() const { return (last_ppage - next_ppage) / PAGE_SIZE; }

std::pair<uint64_t, uint64_t> VirtualMemory::va_to_pa(uint32_t cpu_num, uint64_t vaddr)
{
  auto [ppage, fault] = vpage_to_ppage_map.insert({{cpu_num, vaddr >> LOG2_PAGE_SIZE}, ppage_front()});

  // this vpage doesn't yet have a ppage mapping
  if (fault)
    ppage_pop();

  return {champsim::splice_bits(ppage->second, vaddr, LOG2_PAGE_SIZE), fault ? minor_fault_penalty : 0};
}

std::pair<uint64_t, uint64_t> VirtualMemory::get_pte_pa(uint32_t cpu_num, uint64_t vaddr, std::size_t level)
{
  if (next_pte_page == 0) {
    next_pte_page = ppage_front();
    ppage_pop();
  }

  std::tuple key{cpu_num, vaddr >> shamt(level), level};
  auto [ppage, fault] = page_table.insert({key, next_pte_page});

  // this PTE doesn't yet have a mapping
  if (fault) {
    next_pte_page += pte_page_size;
    if (!(next_pte_page % PAGE_SIZE)) {
      next_pte_page = ppage_front();
      ppage_pop();
    }
  }

  auto offset = get_offset(vaddr, level);
  auto paddr = champsim::splice_bits(ppage->second, offset * PTE_BYTES, champsim::lg2(pte_page_size));
  if constexpr (champsim::debug_print) {
    std::cout << "[VMEM] " << __func__;
    std::cout << " paddr: " << std::hex << paddr;
    std::cout << " vaddr: " << vaddr << std::dec;
    std::cout << " pt_page offset: " << offset;
    std::cout << " translation_level: " << level << std::endl;
  }

  return {paddr, fault ? minor_fault_penalty : 0};
}
