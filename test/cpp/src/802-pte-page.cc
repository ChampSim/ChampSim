#include "catch.hpp"
#include "vmem.h"

#include "champsim_constants.h"
#include "dram_controller.h"
#include "util.h"

SCENARIO("The virtual memory issues references to blocks within a page if they are in the same level") {
  auto pte_page_size = (1ull << 11);
  auto level = GENERATE(2,3,4);

  GIVEN("A large virtual memory") {
    MEMORY_CONTROLLER dram{1, 3200, 12.5, 12.5, 12.5, 7.5};
    VirtualMemory uut{pte_page_size, 5, 200, dram};

    auto size = PAGE_SIZE / pte_page_size;
    WHEN("A full set of requests for PTE entries at level " + std::to_string(level) + " are called for") {
      auto shamt = LOG2_PAGE_SIZE + champsim::lg2(pte_page_size)*level;
      std::vector<uint64_t> given_pages;

      uint64_t req_page = ~(champsim::bitmask(size) << shamt);
      for (std::size_t i = 0; i < size; ++i) {
        given_pages.push_back(uut.get_pte_pa(0, req_page, level).first);
        req_page += 1 << shamt;
      }
      std::sort(std::begin(given_pages), std::end(given_pages));

      THEN("All entries are on the same page") {
        REQUIRE((given_pages.at(0) >> (shamt + champsim::lg2(size))) == (given_pages.at(1) >> (shamt + champsim::lg2(size))));
        REQUIRE((given_pages.front() >> (shamt + champsim::lg2(size))) == (given_pages.back() >> (shamt + champsim::lg2(size))));
      }

      THEN("The entries are spaced by PTE_BYTES") {
        REQUIRE(given_pages.at(1) == given_pages.at(0) + PTE_BYTES);
        REQUIRE(given_pages.back() == given_pages.front() + (PTE_BYTES*(size-1)));
      }

      THEN("No pages are duplicated") {
        auto uniq_end = std::unique(std::begin(given_pages), std::end(given_pages));
        REQUIRE((std::size_t)std::distance(std::begin(given_pages), uniq_end) == size);
      }
    }
  }
}
