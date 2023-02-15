#include "catch.hpp"
#include "vmem.h"

#include <iostream>

#include "dram_controller.h"

SCENARIO("The virtual memory generates a full period of page numbers") {
  GIVEN("A large virtual memory") {
    constexpr unsigned levels = 3;
    constexpr uint64_t pte_page_size = 1ull << 12;
    constexpr auto vmem_size_bits = LOG2_PAGE_SIZE + champsim::lg2(pte_page_size / PTE_BYTES)*levels;
    MEMORY_CONTROLLER dram{1, 3200, 12.5, 12.5, 12.5, 7.5, {}};
    VirtualMemory uut{pte_page_size, levels, 200, dram};

    WHEN("All pages are exhausted") {
      std::vector<uint64_t> given_pages;

      constexpr std::size_t expected_pages = (((1ull << vmem_size_bits) - VMEM_RESERVE_CAPACITY) >> 12);
      uint64_t req_page = pte_page_size;
      for (std::size_t i = 0; i < expected_pages; ++i) {
        given_pages.push_back(uut.va_to_pa(0, req_page).first);
        req_page += pte_page_size;
      }

      std::sort(std::begin(given_pages), std::end(given_pages));

      THEN("No pages are given in the reserved region") {
        REQUIRE(given_pages.front() >= VMEM_RESERVE_CAPACITY);
      }

      AND_THEN("No pages are duplicated") {
        auto uniq_end = std::unique(std::begin(given_pages), std::end(given_pages));
        REQUIRE(std::size(given_pages) == expected_pages);
        REQUIRE(std::distance(std::begin(given_pages), uniq_end) == (signed)expected_pages);
      }
    }
  }
}
