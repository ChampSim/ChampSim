#include "vmem.h"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <random>

#include "champsim_constants.h"
#include "util.h"

VirtualMemory::VirtualMemory(unsigned paddr_bits, uint64_t page_table_page_size, uint32_t page_table_levels, uint64_t random_seed, uint64_t minor_fault_penalty)
    : ppage_free_list(((1ull << (paddr_bits - LOG2_PAGE_SIZE)) - (VMEM_RESERVE_CAPACITY/PAGE_SIZE)), PAGE_SIZE), minor_fault_penalty(minor_fault_penalty), pt_levels(page_table_levels), page_size(page_table_page_size)
{
  assert(page_table_page_size == (1ul << lg2(page_table_page_size)) && page_table_page_size > 1024);

  // populate the free list
  ppage_free_list.front() = VMEM_RESERVE_CAPACITY;
  std::partial_sum(std::cbegin(ppage_free_list), std::cend(ppage_free_list), std::begin(ppage_free_list));

  // then shuffle it
  std::shuffle(std::begin(ppage_free_list), std::end(ppage_free_list), std::mt19937_64{random_seed});

  next_pte_page = ppage_free_list.front();
  ppage_free_list.pop_front();
}

uint64_t VirtualMemory::shamt(uint32_t level) const { return LOG2_PAGE_SIZE + lg2(page_size / PTE_BYTES) * (level); }

uint64_t VirtualMemory::get_offset(uint64_t vaddr, uint32_t level) const { return (vaddr >> shamt(level)) & bitmask(lg2(page_size / PTE_BYTES)); }

std::pair<uint64_t, bool> VirtualMemory::va_to_pa(uint32_t cpu_num, uint64_t vaddr)
{
  auto [ppage, fault] = vpage_to_ppage_map.insert({{cpu_num, vaddr >> LOG2_PAGE_SIZE}, ppage_free_list.front()});

  // this vpage doesn't yet have a ppage mapping
  if (fault)
    ppage_free_list.pop_front();

  return {splice_bits(ppage->second, vaddr, LOG2_PAGE_SIZE), fault};
}

std::pair<uint64_t, bool> VirtualMemory::get_pte_pa(uint32_t cpu_num, uint64_t vaddr, uint32_t level)
{
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

  return {splice_bits(ppage->second, get_offset(vaddr, level) * PTE_BYTES, lg2(page_size)), fault};
}
