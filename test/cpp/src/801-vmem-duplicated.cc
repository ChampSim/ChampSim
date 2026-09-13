#include <catch.hpp>

#include "dram_controller.h"
#include "vmem.h"
#include "defaults.hpp"

SCENARIO("The virtual memory remove PA asked by PTE")
{
  GIVEN("A random page access")
  {
    auto page_number = GENERATE(as<champsim::page_number>{}, 0xdeadbeef, 0x12345678, 0xabcdef01, 0x55555555, 0xaaaaaaaa);
    GIVEN("A large virtual memory") {
      constexpr unsigned levels = 5;
      MEMORY_CONTROLLER dram{champsim::modules::ModuleBuilder{"t801_dram", "DEFAULT_MEMORY_CONTROLLER", champsim::defaults::default_memory_controller()}};
      VirtualMemory uut{champsim::modules::ModuleBuilder{"t801_uut", "DEFAULT_VMEM", champsim::defaults::default_vmem()}
          .add_parameter("dram", static_cast<champsim::modules::memory_controller_module*>(&dram))
          .add_parameter("page_table_levels", static_cast<std::size_t>(levels))};

      //we should re-reference many times at random addresses and ensure that we never allocate additional pages
      std::size_t new_size = uut.available_ppages();
      WHEN("PTE requires memory")
      {
        
        AND_WHEN("PTE ask for a page")
        {
          auto [paddr_a, delay_a] = uut.get_pte_pa(champsim::origin{0, 0}, page_number, 1);

          THEN("The page table missed") { REQUIRE(delay_a > champsim::chrono::clock::duration::zero()); }
          new_size--;
          AND_WHEN("PTE asks for another page")
          {
            auto [paddr_b, delay_b] = uut.get_pte_pa(champsim::origin{0, 0}, page_number, 2);
            new_size--;
            THEN("The page table missed") { REQUIRE(delay_b > champsim::chrono::clock::duration::zero()); }

            THEN("The pages are different") { REQUIRE(paddr_a != paddr_b); }

            THEN("The pages are remove from the available pages") { REQUIRE(new_size == uut.available_ppages()); }
            AND_WHEN("The page is reaccessed") {
              for(int i = 0; i < 10; i++) {
                auto [paddr, delay] = uut.get_pte_pa(champsim::origin{0, 0}, page_number, 2);
                THEN("The page table hit") { REQUIRE(delay == champsim::chrono::clock::duration::zero()); }
                THEN("Another page is not allocated") { REQUIRE(new_size == uut.available_ppages()); }
                THEN("The same page is returned") { REQUIRE(paddr == paddr_b); }
              }
            }
          }
          AND_WHEN("The page is reaccessed") {
            for(int i = 0; i < 10; i++) {
              auto [paddr, delay] = uut.get_pte_pa(champsim::origin{0, 0}, page_number, 1);
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