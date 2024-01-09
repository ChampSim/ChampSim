#include <catch.hpp>
#include "vmem.h"

#include <cmath>

#include "champsim_constants.h"
#include "dram_controller.h"
#include "util/bits.h"

struct AdjDiffMatcher : Catch::Matchers::MatcherGenericBase {
  int64_t stride;

  explicit AdjDiffMatcher(int64_t s) : stride(s) {}

    template<typename Range>
    bool match(Range const& range) const {
      std::vector<int64_t> diffs;
      std::transform(std::next(std::cbegin(range)), std::cend(range), std::cbegin(range), std::back_inserter(diffs), [](auto y, auto x){ return champsim::offset(x,y); });
      return std::all_of(std::next(std::cbegin(diffs)), std::cend(diffs), [stride=stride](auto x){ return x == stride; });
    }

    std::string describe() const override {
        return "has stride " + std::to_string(stride);
    }
};

SCENARIO("The virtual memory issues references to blocks within a page if they are in the same level") {
  auto pte_page_size = (1ull << 11);
  auto level = GENERATE(as<std::size_t>{}, 2,3,4);

  GIVEN("A large virtual memory") {
    MEMORY_CONTROLLER dram{champsim::chrono::picoseconds{3200}, champsim::chrono::picoseconds{12500}, champsim::chrono::picoseconds{12500}, champsim::chrono::picoseconds{12500}, champsim::chrono::picoseconds{7500}, {}};
    VirtualMemory uut{pte_page_size, 5, std::chrono::nanoseconds{6400}, dram};

    champsim::address::difference_type dist{PAGE_SIZE};
    for (std::size_t i = 0; i < level; ++i)
      dist *= pte_page_size;
    std::vector<champsim::address> req_pages{};
    req_pages.push_back(champsim::address{0xcccc000000000000});
    for (std::size_t i = 1; i < (PAGE_SIZE / pte_page_size); ++i)
      req_pages.push_back(req_pages.back() + dist);

    WHEN("A full set of requests for PTE entries at level " + std::to_string(level) + " are called for") {
      std::vector<champsim::address> given_pages{};
      std::transform(std::cbegin(req_pages), std::cend(req_pages), std::back_inserter(given_pages), [&](auto req_page){ return uut.get_pte_pa(0, req_page, level).first; });
      std::sort(std::begin(given_pages), std::end(given_pages));

      THEN("All entries are on the same page") {
        std::vector<champsim::page_number> pages;
        std::transform(std::cbegin(given_pages), std::cend(given_pages), std::back_inserter(pages), [](auto x){ return champsim::page_number{x}; });
        REQUIRE_THAT(pages, AdjDiffMatcher{0});
      }

      THEN("The entries are spaced by pte_page_size") {
        REQUIRE_THAT(given_pages, AdjDiffMatcher{(int64_t)pte_page_size});
      }
    }
  }
}
