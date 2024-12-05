#include <catch.hpp>
#include "matchers.hpp"
#include "vmem.h"

#include <cmath>

#include "dram_controller.h"
#include "util/bits.h"

SCENARIO("The virtual memory issues references to blocks within a page if they are in the same level") {
  champsim::data::bytes pte_page_size{1LL << 11};
  auto level = GENERATE(as<std::size_t>{}, 2,3,4);

  GIVEN("A large virtual memory") {
    MEMORY_CONTROLLER dram{champsim::chrono::picoseconds{3200}, champsim::chrono::picoseconds{6400}, std::size_t{18}, std::size_t{18}, std::size_t{18}, std::size_t{38}, champsim::chrono::microseconds{64000}, {}, 64, 64, 1, champsim::data::bytes{8}, 1024, 1024, 4, 4, 4, 8192};
    VirtualMemory uut{pte_page_size, 5, std::chrono::nanoseconds{6400}, dram};

    champsim::data::bytes dist{1};
    for (std::size_t i = 0; i < level; ++i)
      dist *= pte_page_size.count();
    std::vector<champsim::page_number> req_pages{};
    req_pages.push_back(champsim::page_number{0xcccc000000000});
    for (auto i = pte_page_size; i < champsim::data::bytes{PAGE_SIZE}; i += pte_page_size)
      req_pages.push_back(req_pages.back() + dist);

    WHEN("A full set of requests for PTE entries at level " + std::to_string(level) + " are called for") {
      std::vector<champsim::address> given_pages{};
      std::transform(std::cbegin(req_pages), std::cend(req_pages), std::back_inserter(given_pages), [&](auto req_page){ return uut.get_pte_pa(0, req_page, level).first; });
      std::sort(std::begin(given_pages), std::end(given_pages));

      THEN("All entries are on the same page") {
        std::vector<champsim::page_number> pages;
        std::transform(std::cbegin(given_pages), std::cend(given_pages), std::back_inserter(pages), [](auto x){ return champsim::page_number{x}; });
        REQUIRE_THAT(pages, champsim::test::StrideMatcher<champsim::page_number>{0});
      }

      THEN("The entries are spaced by pte_page_size") {
        REQUIRE_THAT(given_pages, champsim::test::StrideMatcher<champsim::address>{pte_page_size.count()});
      }
    }
  }
}
