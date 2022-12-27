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

#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>
#include <random>

#include "champsim.h"
#include "util.h"

VirtualMemory::VirtualMemory(uint64_t capacity, uint64_t pg_size, uint32_t page_table_levels, uint64_t random_seed, uint64_t minor_fault_penalty)
    : minor_fault_penalty(minor_fault_penalty), pt_levels(page_table_levels), page_size(pg_size),
      ppage_free_list((capacity - VMEM_RESERVE_CAPACITY) / PAGE_SIZE, PAGE_SIZE)
{
  assert(capacity % PAGE_SIZE == 0);
  assert(pg_size == (1ul << lg2(pg_size)) && pg_size > 1024);

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
