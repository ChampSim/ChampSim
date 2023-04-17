#include <catch.hpp>
#include "vmem.h"

#include "dram_controller.h"

SCENARIO("The virtual memory remove PA asked by PTE") {
  GIVEN("A large virtual memory") {
    constexpr unsigned levels = 5;
    constexpr uint64_t pte_page_size = 1ull << 12;
    MEMORY_CONTROLLER dram{1, 3200, 12.5, 12.5, 12.5, 7.5, {}};
    VirtualMemory uut{pte_page_size, levels, 200, dram};

    WHEN("PTE requires memory") {
      std::size_t original_size = uut.available_ppages();

      AND_WHEN("PTE ask for a page") {
        auto [paddr_a, delay_a] = uut.get_pte_pa(0, 0, 1);

        THEN("The page table missed") {
          REQUIRE(delay_a > 0);
        }

        AND_WHEN("PTE asks for another page") {
          auto [paddr_b, delay_b] = uut.get_pte_pa(0, 0, 2);

          THEN("The page table missed") {
            REQUIRE(delay_b > 0);
          }

          THEN("The pages are different") {
            REQUIRE(paddr_a != paddr_b);
          }

          THEN("The pages are remove from the available pages") {
            // an additional one because it should remove one extra page
            REQUIRE(original_size - 3 == uut.available_ppages());
          }
        }
      }
    }
  }
}

