#include <catch.hpp>

#include "dram_controller.h"
#include "vmem.h"
#include "defaults.hpp"

TEST_CASE("The virtual memory evaluates the correct shift amounts")
{
  constexpr unsigned log2_pte_page_size = 12;

  auto level = GENERATE(as<std::size_t>{}, 1, 2, 3, 4, 5);

  MEMORY_CONTROLLER dram{champsim::modules::ModuleBuilder{"t803_dram_0", "DEFAULT_MEMORY_CONTROLLER", champsim::defaults::default_memory_controller()}};
  VirtualMemory uut{champsim::modules::ModuleBuilder{"t803_uut_0", "DEFAULT_VMEM", champsim::defaults::default_vmem()}
      .add_parameter("dram", static_cast<champsim::modules::memory_controller_module*>(&dram))
      .add_parameter("page_table_page_size", champsim::data::bytes{1ul << log2_pte_page_size})};

  const auto log2_page_size = champsim::modules::ModuleBuilder::globals().get_parameter<unsigned>("log2_page_size");
  champsim::data::bits expected_value{log2_page_size + (log2_pte_page_size - champsim::lg2(pte_entry::byte_multiple)) * (level - 1)};
  REQUIRE(uut.shamt(level) == expected_value);
}

TEST_CASE("The virtual memory evaluates the correct offsets")
{
  constexpr std::size_t log2_pte_page_size = 12;

  auto level = GENERATE(as<unsigned>{}, 1, 2, 3, 4, 5);

  MEMORY_CONTROLLER dram{champsim::modules::ModuleBuilder{"t803_dram_1", "DEFAULT_MEMORY_CONTROLLER", champsim::defaults::default_memory_controller()}};
  VirtualMemory uut{champsim::modules::ModuleBuilder{"t803_uut_1", "DEFAULT_VMEM", champsim::defaults::default_vmem()}
      .add_parameter("dram", static_cast<champsim::modules::memory_controller_module*>(&dram))
      .add_parameter("page_table_page_size", champsim::data::bytes{1ul << log2_pte_page_size})};

  const auto log2_page_size = champsim::modules::ModuleBuilder::globals().get_parameter<unsigned>("log2_page_size");
  champsim::address addr{(0xffff'ffff'ffe0'0000 | (level << log2_page_size)) << ((level - 1) * 9)};
  REQUIRE(uut.get_offset(addr, level) == level);
}
