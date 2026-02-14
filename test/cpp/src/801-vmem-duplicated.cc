#include <catch.hpp>

#include "dram_controller.h"
#include "vmem.h"

SCENARIO("The virtual memory remove PA asked by PTE")
{
  GIVEN("A random page access")
  {
    auto page_number = GENERATE(as<champsim::page_number>{}, 0xdeadbeef, 0x12345678, 0xabcdef01, 0x55555555, 0xaaaaaaaa);
    GIVEN("A large virtual memory") {
      constexpr unsigned levels = 5;
      constexpr champsim::data::bytes pte_page_size{1ull << 12};
      MEMORY_CONTROLLER dram{champsim::chrono::picoseconds{3200},
                            champsim::chrono::picoseconds{6400},
                            std::size_t{18},
                            std::size_t{18},
                            std::size_t{18},
                            std::size_t{38},
                            champsim::chrono::microseconds{64000},
                            {},
                            64,
                            64,
                            1,
                            champsim::data::bytes{8},
                            1024,
                            1024,
                            4,
                            4,
                            4,
                            8192};
      VirtualMemory uut{pte_page_size, levels, std::chrono::nanoseconds{6400}, dram};

      //we should re-reference many times at random addresses and ensure that we never allocate additional pages
      std::size_t new_size = uut.available_ppages();
      WHEN("PTE requires memory")
      {
        
        AND_WHEN("PTE ask for a page")
        {
          auto [paddr_a, delay_a] = uut.get_pte_pa(0, page_number, 1);

          THEN("The page table missed") { REQUIRE(delay_a > champsim::chrono::clock::duration::zero()); }
          new_size--;
          AND_WHEN("PTE asks for another page")
          {
            auto [paddr_b, delay_b] = uut.get_pte_pa(0, page_number, 2);
            new_size--;
            THEN("The page table missed") { REQUIRE(delay_b > champsim::chrono::clock::duration::zero()); }

            THEN("The pages are different") { REQUIRE(paddr_a != paddr_b); }

            THEN("The pages are remove from the available pages") { REQUIRE(new_size == uut.available_ppages()); }
            AND_WHEN("The page is reaccessed") {
              for(int i = 0; i < 10; i++) {
                auto [paddr, delay] = uut.get_pte_pa(0, page_number, 2);
                THEN("The page table hit") { REQUIRE(delay == champsim::chrono::clock::duration::zero()); }
                THEN("Another page is not allocated") { REQUIRE(new_size == uut.available_ppages()); }
                THEN("The same page is returned") { REQUIRE(paddr == paddr_b); }
              }
            }
          }
          AND_WHEN("The page is reaccessed") {
            for(int i = 0; i < 10; i++) {
              auto [paddr, delay] = uut.get_pte_pa(0, page_number, 1);
              THEN("The page table hit") { REQUIRE(delay == champsim::chrono::clock::duration::zero()); }
              THEN("Another page is not allocated") { REQUIRE(new_size == uut.available_ppages()); }
              THEN("The same page is returned") { REQUIRE(paddr == paddr_a); }
            }
          }
        }
      }
    }
  }
}