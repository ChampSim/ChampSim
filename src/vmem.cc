#include "vmem.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>

#include "champsim.h"
#include "champsim_constants.h"
#include "dram_controller.h"
#include "util/bits.h"

VirtualMemory::VirtualMemory(uint64_t page_table_page_size, std::size_t page_table_levels, uint64_t minor_penalty, MEMORY_CONTROLLER& dram)
    : next_ppage(VMEM_RESERVE_CAPACITY), last_ppage(1ull << (LOG2_PAGE_SIZE + champsim::lg2(page_table_page_size / PTE_BYTES) * page_table_levels)),
      minor_fault_penalty(minor_penalty), pt_levels(page_table_levels), pte_page_size(page_table_page_size)
{
  assert(page_table_page_size > 1024);
  assert(page_table_page_size == (1ull << champsim::lg2(page_table_page_size)));
  assert(last_ppage > VMEM_RESERVE_CAPACITY);

  auto required_bits = champsim::lg2(last_ppage);
  if (required_bits > 64)
    std::cout << "WARNING: virtual memory configuration would require " << required_bits << " bits of addressing." << std::endl;
  if (required_bits > champsim::lg2(dram.size()))
    std::cout << "WARNING: physical memory size is smaller than virtual memory size" << std::endl;
}

uint64_t VirtualMemory::shamt(std::size_t level) const { return LOG2_PAGE_SIZE + champsim::lg2(pte_page_size / PTE_BYTES) * (level - 1); }

uint64_t VirtualMemory::get_offset(champsim::address vaddr, std::size_t level) const
{
  const auto lower = shamt(level);;
  return vaddr.slice(lower+champsim::lg2(pte_page_size / PTE_BYTES), lower).to<uint64_t>();
}

champsim::address VirtualMemory::ppage_front() const
{
  assert(available_ppages() > 0);
  return next_ppage;
}

void VirtualMemory::ppage_pop() { next_ppage += PAGE_SIZE; }

std::size_t VirtualMemory::available_ppages() const { return champsim::offset(next_ppage, last_ppage) / PAGE_SIZE; }

std::pair<champsim::address, uint64_t> VirtualMemory::va_to_pa(uint32_t cpu_num, champsim::address vaddr)
{
  std::pair key{cpu_num, champsim::page_number{vaddr}};
  auto [ppage, fault] = vpage_to_ppage_map.insert(std::pair{key, ppage_front()});

  // this vpage doesn't yet have a ppage mapping
  if (fault)
    ppage_pop();

  return {champsim::splice(champsim::page_number{ppage->second}, champsim::page_offset{vaddr}), fault ? minor_fault_penalty : 0};
}

std::pair<champsim::address, uint64_t> VirtualMemory::get_pte_pa(uint32_t cpu_num, champsim::address vaddr, std::size_t level)
{
  if (next_pte_page == champsim::address{}) {
    next_pte_page = ppage_front();
    ppage_pop();
  }

  std::tuple key{cpu_num, level, vaddr.slice_upper(shamt(level))};
  auto [ppage, fault] = page_table.insert(std::pair{key, next_pte_page});

  // this PTE doesn't yet have a mapping
  if (fault) {
    next_pte_page += pte_page_size;
    if (champsim::page_offset{next_pte_page} == champsim::page_offset{0}) {
      next_pte_page = ppage_front();
      ppage_pop();
    }
  }

  auto offset = get_offset(vaddr, level);
  auto paddr = ppage->second + (offset * PTE_BYTES);
  if constexpr (champsim::debug_print) {
    std::cout << "[VMEM] " << __func__;
    std::cout << " paddr: " << paddr;
    std::cout << " vaddr: " << vaddr;
    std::cout << " pt_page offset: " << offset;
    std::cout << " translation_level: " << level;
    if (fault)
      std::cout << " PAGE FAULT ";
    std::cout << std::endl;
  }

  return {paddr, fault ? minor_fault_penalty : 0};
}
