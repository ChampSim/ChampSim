#include "vmem.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>

#include "champsim.h"
#include "champsim_constants.h"
#include "util.h"

VirtualMemory::VirtualMemory(unsigned paddr_bits, uint64_t page_table_page_size, std::size_t page_table_levels, uint64_t minor_penalty, MEMORY_CONTROLLER& dram)
    : ppage_free_list(((1ull << (paddr_bits - LOG2_PAGE_SIZE)) - (VMEM_RESERVE_CAPACITY / PAGE_SIZE)), PAGE_SIZE), minor_fault_penalty(minor_penalty),
      pt_levels(page_table_levels), pte_page_size(page_table_page_size)
{
  assert(page_table_page_size == (1ul << champsim::lg2(page_table_page_size)) && page_table_page_size > 1024);

  if (paddr_bits > champsim::lg2(dram.size()))
    std::cout << "WARNING: physical memory size is smaller than virtual memory size" << std::endl;

  // populate the free list
  ppage_free_list.front() = VMEM_RESERVE_CAPACITY;
  std::partial_sum(std::cbegin(ppage_free_list), std::cend(ppage_free_list), std::begin(ppage_free_list));
}

uint64_t VirtualMemory::shamt(std::size_t level) const { return LOG2_PAGE_SIZE + champsim::lg2(pte_page_size / PTE_BYTES) * (level - 1); }

uint64_t VirtualMemory::get_offset(uint64_t vaddr, std::size_t level) const
{
  return (vaddr >> shamt(level)) & champsim::bitmask(champsim::lg2(pte_page_size / PTE_BYTES));
}

std::pair<uint64_t, uint64_t> VirtualMemory::va_to_pa(uint16_t asid, uint64_t vaddr)
{
  auto [ppage, fault] = vpage_to_ppage_map.insert({{asid, vaddr >> LOG2_PAGE_SIZE}, ppage_free_list.front()});

  // this vpage doesn't yet have a ppage mapping
  if (fault)
    ppage_free_list.pop_front();

  return {champsim::splice_bits(ppage->second, vaddr, LOG2_PAGE_SIZE), fault ? minor_fault_penalty : 0};
}

std::pair<uint64_t, uint64_t> VirtualMemory::get_pte_pa(uint16_t asid, uint64_t vaddr, std::size_t level)
{
  if (next_pte_page == 0) {
    next_pte_page = ppage_free_list.front();
    ppage_free_list.pop_front();
  }

  std::tuple key{asid, vaddr >> shamt(level+1), level};
  auto [ppage, fault] = page_table.insert({key, next_pte_page});

  // this PTE doesn't yet have a mapping
  if (fault) {
    next_pte_page += pte_page_size;
    if (next_pte_page % PAGE_SIZE) {
      next_pte_page = ppage_free_list.front();
      ppage_free_list.pop_front();
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
