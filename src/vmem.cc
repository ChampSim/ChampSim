/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "vmem.h"

#include <cassert>

#include "champsim.h"
#include "champsim_constants.h"
#include "dram_controller.h"
#include "util/bits.h"

VirtualMemory::VirtualMemory(uint64_t page_table_page_size, std::size_t page_table_levels, uint64_t minor_penalty, MEMORY_CONTROLLER& dram)
    : next_pte_page(LOG2_PAGE_SIZE, champsim::lg2(page_table_page_size), 0), last_ppage(1ull << (champsim::lg2(page_table_page_size / PTE_BYTES) * page_table_levels)),
      minor_fault_penalty(minor_penalty), pt_levels(page_table_levels), pte_page_size(page_table_page_size)
{
  assert(page_table_page_size > 1024);
  assert(page_table_page_size == (1ull << champsim::lg2(page_table_page_size)));
  assert(last_ppage > next_ppage);

  auto required_bits = LOG2_PAGE_SIZE + champsim::lg2(last_ppage.to<uint64_t>());
  if (required_bits > champsim::address::bits)
    std::cout << "WARNING: virtual memory configuration would require " << required_bits << " bits of addressing." << std::endl;
  if (required_bits > champsim::lg2(dram.size()))
    std::cout << "WARNING: physical memory size is smaller than virtual memory size" << std::endl;
}

uint64_t VirtualMemory::shamt(std::size_t level) const { return LOG2_PAGE_SIZE + champsim::lg2(pte_page_size / PTE_BYTES) * (level - 1); }

uint64_t VirtualMemory::get_offset(champsim::address vaddr, std::size_t level) const
{
  const auto lower = shamt(level);
  return vaddr.slice(lower+champsim::lg2(pte_page_size / PTE_BYTES), lower).to<uint64_t>();
}

champsim::page_number VirtualMemory::ppage_front() const
{
  assert(available_ppages() > 0);
  return next_ppage;
}

void VirtualMemory::ppage_pop() { ++next_ppage; }

std::size_t VirtualMemory::available_ppages() const {
  assert(next_ppage <= last_ppage);
  return static_cast<std::size_t>(champsim::offset(next_ppage, last_ppage)); // Cast protected by prior assert
}

std::pair<champsim::address, uint64_t> VirtualMemory::va_to_pa(uint32_t cpu_num, champsim::address vaddr)
{
  auto [ppage, fault] = vpage_to_ppage_map.try_emplace({cpu_num, champsim::page_number{vaddr}}, ppage_front());

  // this vpage doesn't yet have a ppage mapping
  if (fault)
    ppage_pop();

  return {champsim::splice(champsim::page_number{ppage->second}, champsim::page_offset{vaddr}), fault ? minor_fault_penalty : 0};
}

std::pair<champsim::address, uint64_t> VirtualMemory::get_pte_pa(uint32_t cpu_num, champsim::address vaddr, std::size_t level)
{
  if (champsim::page_offset{next_pte_page} == champsim::page_offset{0}) {
    active_pte_page = ppage_front();
    ppage_pop();
  }

  auto [ppage, fault] = page_table.try_emplace({cpu_num, level, vaddr.slice_upper(shamt(level))}, champsim::splice(active_pte_page, next_pte_page));

  // this PTE doesn't yet have a mapping
  if (fault)
    next_pte_page++;

  auto offset = get_offset(vaddr, level);
  champsim::address paddr{champsim::splice(ppage->second, champsim::address_slice{champsim::lg2(pte_page_size), champsim::lg2(PTE_BYTES), offset})};
  if constexpr (champsim::debug_print) {
    std::cout << "[VMEM] " << __func__;
    std::cout << " paddr: " << paddr;
    std::cout << " vaddr: " << vaddr;
    std::cout << " pt_page offset: " << offset;
    std::cout << " translation_level: " << level;
    if (fault)
      std::cout << " PAGE FAULT";
    std::cout << std::endl;
  }

  return {paddr, fault ? minor_fault_penalty : 0};
}
