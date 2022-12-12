#include "catch.hpp"
#include "vmem.h"

#include "champsim_constants.h"
#include "dram_controller.h"
#include "util/bits.h"

SCENARIO("The virtual memory issues references to blocks within a page if they are in the same level") {
  constexpr unsigned vmem_size_bits = 34;

  auto pte_page_size = (1ull << 11);
  auto level = GENERATE(2,3,4);

  GIVEN("A large virtual memory") {
    MEMORY_CONTROLLER dram{1, 3200, 12.5, 12.5, 12.5, 7.5};
    VirtualMemory uut{vmem_size_bits, pte_page_size, 5, 200, dram};

    auto size = PAGE_SIZE / pte_page_size;
    WHEN("A full set of requests for PTE entries at level " + std::to_string(level) + " are called for") {
      auto shamt = LOG2_PAGE_SIZE + champsim::lg2(pte_page_size)*level;
      std::vector<champsim::address> given_pages;

      champsim::address req_page{~(champsim::bitmask(size) << shamt)};
      for (std::size_t i = 0; i < size; ++i) {
        given_pages.push_back(uut.get_pte_pa(0, req_page, level).first);
        req_page += 1ull << shamt;
      }
      std::sort(std::begin(given_pages), std::end(given_pages));

      THEN("All entries are on the same page") {
        REQUIRE(given_pages.at(0).slice_upper(shamt + champsim::lg2(size)) == given_pages.at(1).slice_upper(shamt + champsim::lg2(size)));
        REQUIRE(given_pages.front().slice_upper(shamt + champsim::lg2(size)) == given_pages.back().slice_upper(shamt + champsim::lg2(size)));
      }

      THEN("The entries are spaced by pte_page_size") {
        REQUIRE(champsim::offset(given_pages.at(0), given_pages.at(1)) == static_cast<champsim::address::difference_type>(pte_page_size));
        REQUIRE(champsim::offset(given_pages.front(), given_pages.back()) == static_cast<champsim::address::difference_type>(pte_page_size*(size-1)));
      }

      THEN("No pages are duplicated") {
        auto uniq_end = std::unique(std::begin(given_pages), std::end(given_pages));
        REQUIRE(std::distance(std::begin(given_pages), uniq_end) == static_cast<std::ptrdiff_t>(size));
      }
    }
  }
}
