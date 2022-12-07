#include "catch.hpp"
#include "vmem.h"

#include <iostream>

#include "dram_controller.h"

SCENARIO("The virtual memory generates a full period of page numbers") {
  GIVEN("A large virtual memory") {
    constexpr unsigned vmem_size_bits = 33;
    MEMORY_CONTROLLER dram{1, 3200, 12.5, 12.5, 12.5, 7.5};
    VirtualMemory uut{vmem_size_bits, 1 << 12, 5, 200, dram};

    WHEN("All pages are exhausted") {
      std::vector<champsim::address> given_pages;

      constexpr std::size_t expected_pages = (((1ull << vmem_size_bits) - VMEM_RESERVE_CAPACITY) >> 12);
      champsim::address req_page{1 << 12};
      for (std::size_t i = 0; i < expected_pages; ++i) {
        given_pages.push_back(uut.va_to_pa(0, req_page).first);
        req_page += (1 << 12);
      }

      std::sort(std::begin(given_pages), std::end(given_pages));

      THEN("No pages are given in the reserved region") {
        REQUIRE(given_pages.front() >= champsim::address{VMEM_RESERVE_CAPACITY});
      }

      AND_THEN("No pages are duplicated") {
        auto uniq_end = std::unique(std::begin(given_pages), std::end(given_pages));
        REQUIRE(std::size(given_pages) == expected_pages);
        REQUIRE(std::distance(std::begin(given_pages), uniq_end) == (signed)expected_pages);
      }
    }
  }
}
