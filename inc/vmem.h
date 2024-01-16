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
#include <map>

#include "address.h"
#include "champsim.h"

class MEMORY_CONTROLLER;

using pte_entry = champsim::data::size<long long, std::ratio<8>>;

class VirtualMemory
{
private:
  std::map<std::pair<uint32_t, champsim::page_number>, champsim::address> vpage_to_ppage_map;
  std::map<std::tuple<uint32_t, uint32_t, champsim::address_slice<champsim::dynamic_extent>>, champsim::address> page_table;

public:
  const uint64_t minor_fault_penalty;
  const std::size_t pt_levels;
  const pte_entry pte_page_size; // Size of a PTE page

private:
  champsim::page_number active_pte_page{};
  champsim::address_slice<champsim::dynamic_extent> next_pte_page;

  champsim::page_number next_ppage;
  champsim::page_number last_ppage;

  [[nodiscard]] champsim::page_number ppage_front() const;
  void ppage_pop();

public:
  // capacity and pg_size are measured in bytes, and capacity must be a multiple of pg_size
  VirtualMemory(champsim::data::bytes page_table_page_size, std::size_t page_table_levels, uint64_t minor_penalty, MEMORY_CONTROLLER& dram);
  [[nodiscard]] uint64_t shamt(std::size_t level) const;
  [[nodiscard]] uint64_t get_offset(champsim::address vaddr, std::size_t level) const;
  [[nodiscard]] std::size_t available_ppages() const;
  std::pair<champsim::address, uint64_t> va_to_pa(uint32_t cpu_num, champsim::address vaddr);
  std::pair<champsim::address, uint64_t> get_pte_pa(uint32_t cpu_num, champsim::address vaddr, std::size_t level);
};

#endif
