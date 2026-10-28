#include <catch.hpp>

#include "defaults.hpp"
#include "dram_controller.h"
#include "origin.h"
#include "vmem.h"

SCENARIO("Virtual memory keys address spaces by the origin's stream, not its consumer")
{
  GIVEN("A virtual memory")
  {
    MEMORY_CONTROLLER dram{champsim::modules::ModuleBuilder{"t803_dram", "DEFAULT_MEMORY_CONTROLLER", champsim::defaults::default_memory_controller()}};
    VirtualMemory uut{champsim::modules::ModuleBuilder{"t803_uut", "DEFAULT_VMEM", champsim::defaults::default_vmem()}
        .add_parameter("dram", static_cast<champsim::modules::memory_controller_module*>(&dram))
        .add_parameter("page_table_levels", static_cast<std::size_t>(5))};

    const champsim::page_number vaddr{0xdeadbeef};

    WHEN("The same virtual page is translated under two different streams")
    {
      auto [page_a, delay_a] = uut.va_to_pa(champsim::origin{0, 0}, vaddr);
      auto [page_b, delay_b] = uut.va_to_pa(champsim::origin{1, 1}, vaddr);

      THEN("Each stream is its own address space: the physical pages differ")
      {
        REQUIRE(page_a != page_b);
      }
    }

    WHEN("The same virtual page is translated under the same stream from two different consumers")
    {
      auto [page_a, delay_a] = uut.va_to_pa(champsim::origin{0, 5}, vaddr);
      auto [page_b, delay_b] = uut.va_to_pa(champsim::origin{1, 5}, vaddr);

      THEN("One address space is shared: the physical pages match")
      {
        REQUIRE(page_a == page_b);
        THEN("And the second lookup is a hit (no page fault delay)")
        {
          REQUIRE(delay_b == champsim::chrono::clock::duration::zero());
        }
      }
    }
  }
}
