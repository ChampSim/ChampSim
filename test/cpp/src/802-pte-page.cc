#include <catch.hpp>
#include "vmem.h"

#include <cmath>

#include "champsim_constants.h"
#include "dram_controller.h"

struct AdjDiffMatcher : Catch::Matchers::MatcherGenericBase {
  uint64_t stride;

  explicit AdjDiffMatcher(uint64_t s) : stride(s) {}

    template<typename Range>
    bool match(Range const& range) const {
      std::vector<uint64_t> diffs;
      std::adjacent_difference(std::cbegin(range), std::cend(range), std::back_inserter(diffs));
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
    MEMORY_CONTROLLER dram{1, 3200, 12.5, 12.5, 12.5, 7.5, {}};
    VirtualMemory uut{pte_page_size, 5, 200, dram};

    uint64_t dist = PAGE_SIZE;
    for (std::size_t i = 0; i < level; ++i)
      dist *= pte_page_size;
    std::vector<uint64_t> req_pages(PAGE_SIZE / pte_page_size, dist);
    req_pages.front() = 0xcccc000000000000;
    std::partial_sum(std::cbegin(req_pages), std::cend(req_pages), std::begin(req_pages));

    WHEN("A full set of requests for PTE entries at level " + std::to_string(level) + " are called for") {
      std::vector<uint64_t> given_pages{};
      std::transform(std::cbegin(req_pages), std::cend(req_pages), std::back_inserter(given_pages), [&](auto req_page){ return uut.get_pte_pa(0, req_page, level).first; });
      std::sort(std::begin(given_pages), std::end(given_pages));

      THEN("All entries are on the same page") {
        std::vector<uint64_t> pages;
        std::transform(std::cbegin(given_pages), std::cend(given_pages), std::back_inserter(pages), [](auto x){ return x >> LOG2_PAGE_SIZE; });
        REQUIRE_THAT(pages, AdjDiffMatcher{0});
      }

      THEN("The entries are spaced by pte_page_size") {
        REQUIRE_THAT(given_pages, AdjDiffMatcher{pte_page_size});
      }
    }
  }
}
