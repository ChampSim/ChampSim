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

#ifndef VMEM_H
#define VMEM_H

#include <cstdint>
#include <deque>
#include <map>

#include "champsim_constants.h"

class MEMORY_CONTROLLER;

// reserve 1MB or one page of space
inline constexpr auto VMEM_RESERVE_CAPACITY = std::max<uint64_t>(PAGE_SIZE, 1ull << 20);

inline constexpr std::size_t PTE_BYTES = 8;

class VirtualMemory
{
private:
  std::map<std::pair<uint32_t, uint64_t>, uint64_t> vpage_to_ppage_map;
  std::map<std::tuple<uint32_t, uint64_t, uint32_t>, uint64_t> page_table;

  uint64_t next_pte_page = 0;

  uint64_t next_ppage;
  uint64_t last_ppage;

  uint64_t ppage_front() const;
  void ppage_pop();

public:
  const uint64_t minor_fault_penalty;
  const std::size_t pt_levels;
  const uint64_t pte_page_size; // Size of a PTE page

  // capacity and pg_size are measured in bytes, and capacity must be a multiple of pg_size
  VirtualMemory(uint64_t pg_size, std::size_t page_table_levels, uint64_t minor_penalty, MEMORY_CONTROLLER& dram);
  uint64_t shamt(std::size_t level) const;
  uint64_t get_offset(uint64_t vaddr, std::size_t level) const;
  std::size_t available_ppages() const;
  std::pair<uint64_t, uint64_t> va_to_pa(uint32_t cpu_num, uint64_t vaddr);
  std::pair<uint64_t, uint64_t> get_pte_pa(uint32_t cpu_num, uint64_t vaddr, std::size_t level);
};

#endif
