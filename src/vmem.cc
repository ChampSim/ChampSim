#include "vmem.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>

#include "champsim_constants.h"
#include "util.h"

VirtualMemory::VirtualMemory(unsigned paddr_bits, uint64_t page_table_page_size, uint32_t page_table_levels, uint64_t minor_fault_penalty,
                             MEMORY_CONTROLLER& dram)
    : ppage_free_list(((1ull << (paddr_bits - LOG2_PAGE_SIZE)) - (VMEM_RESERVE_CAPACITY / PAGE_SIZE)), PAGE_SIZE), minor_fault_penalty(minor_fault_penalty),
      pt_levels(page_table_levels), page_size(page_table_page_size)
{
  assert(page_table_page_size == (1ul << champsim::lg2(page_table_page_size)) && page_table_page_size > 1024);

  if (paddr_bits > champsim::lg2(dram.size()))
    std::cout << "WARNING: physical memory size is smaller than virtual memory size" << std::endl;

  // populate the free list
  ppage_free_list.front() = VMEM_RESERVE_CAPACITY;
  std::partial_sum(std::cbegin(ppage_free_list), std::cend(ppage_free_list), std::begin(ppage_free_list));
}

uint64_t VirtualMemory::shamt(uint32_t level) const { return LOG2_PAGE_SIZE + champsim::lg2(page_size / PTE_BYTES) * (level); }

uint64_t VirtualMemory::get_offset(uint64_t vaddr, uint32_t level) const { return (vaddr >> shamt(level)) & champsim::bitmask(champsim::lg2(page_size / PTE_BYTES)); }

std::pair<uint64_t, uint64_t> VirtualMemory::va_to_pa(uint32_t cpu_num, uint64_t vaddr)
{
  auto [ppage, fault] = vpage_to_ppage_map.insert({{cpu_num, vaddr >> LOG2_PAGE_SIZE}, ppage_free_list.front()});

  // this vpage doesn't yet have a ppage mapping
  if (fault)
    ppage_free_list.pop_front();

  return {champsim::splice_bits(ppage->second, vaddr, LOG2_PAGE_SIZE), fault ? minor_fault_penalty : 0};
}

std::pair<uint64_t, uint64_t> VirtualMemory::get_pte_pa(uint32_t cpu_num, uint64_t vaddr, uint32_t level)
{
  if (next_pte_page == 0) {
    next_pte_page = ppage_free_list.front();
    ppage_free_list.pop_front();
  }

  std::tuple key{cpu_num, vaddr >> shamt(level + 1), level};
  auto [ppage, fault] = page_table.insert({key, next_pte_page});

  // this PTE doesn't yet have a mapping
  if (fault) {
    next_pte_page += page_size;
    if (next_pte_page % PAGE_SIZE) {
      next_pte_page = ppage_free_list.front();
      ppage_free_list.pop_front();
    }
  }

  return {champsim::splice_bits(ppage->second, get_offset(vaddr, level) * PTE_BYTES, champsim::lg2(page_size)), fault ? minor_fault_penalty : 0};
}
