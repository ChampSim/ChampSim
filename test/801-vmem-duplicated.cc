#include "catch.hpp"
#include "vmem.h"

#include <iostream>

SCENARIO("The virtual memory remove PA asked by PTE") {
  GIVEN("A large virtual memory") {
    constexpr unsigned vmem_size_bits = 33;
    MEMORY_CONTROLLER dram{1, 3200, 12.5, 12.5, 12.5, 7.5};
    VirtualMemory uut{vmem_size_bits, 1 << 12, 5, 200, dram};

    WHEN("PTE requires memory") {
      std::size_t original_size = uut.available_ppages();
      int pa_used = 0;

      WHEN("PTE ask for two pages") {
        if (uut.get_pte_pa(0, 0, 1).second != 0) pa_used++;
        if (uut.get_pte_pa(0, 0, 2).second != 0) pa_used++;

        THEN("The pages are remove from the available pages") {
          REQUIRE(pa_used > 0);
          // Minus one because it should remove one extra page
          REQUIRE(original_size - pa_used - 1 == uut.available_ppages());
        }
      }
    }
  }
}

