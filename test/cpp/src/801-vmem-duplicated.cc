#include <catch.hpp>
#include "vmem.h"

#include "dram_controller.h"

SCENARIO("The virtual memory remove PA asked by PTE") {
  GIVEN("A large virtual memory") {
    constexpr unsigned levels = 5;
    constexpr champsim::data::bytes pte_page_size{1ull << 12};
    MEMORY_CONTROLLER dram{champsim::chrono::picoseconds{3200}, champsim::chrono::picoseconds{12500}, champsim::chrono::picoseconds{12500}, champsim::chrono::picoseconds{12500}, champsim::chrono::picoseconds{7500}, {}, 64, 64, 1, champsim::data::bytes{1}, 1, 1, 1, 1};
    VirtualMemory uut{pte_page_size, levels, std::chrono::nanoseconds{6400}, dram};

    WHEN("PTE requires memory") {
      std::size_t original_size = uut.available_ppages();

      const champsim::page_number to_check{0xdeadbeef};
      AND_WHEN("PTE ask for a page") {
        auto [paddr_a, delay_a] = uut.get_pte_pa(0, to_check, 1);

        THEN("The page table missed") {
          REQUIRE(delay_a > champsim::chrono::clock::duration::zero());
        }

        AND_WHEN("PTE asks for another page") {
          auto [paddr_b, delay_b] = uut.get_pte_pa(0, to_check, 2);

          THEN("The page table missed") {
            REQUIRE(delay_b > champsim::chrono::clock::duration::zero());
          }

          THEN("The pages are different") {
            REQUIRE(paddr_a != paddr_b);
          }

          THEN("The pages are remove from the available pages") {
            REQUIRE(original_size - 2 == uut.available_ppages());
          }
        }
      }
    }
  }
}

