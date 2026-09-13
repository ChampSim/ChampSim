#include <catch.hpp>
#include <cmath>

#include "dram_controller.h"
#include "matchers.hpp"
#include "util/bits.h"
#include "vmem.h"
#include "defaults.hpp"

SCENARIO("The virtual memory issues references to blocks within a page if they are in the same level")
{
  champsim::data::bytes pte_page_size{1LL << 11};
  auto level = GENERATE(as<std::size_t>{}, 2, 3, 4);

  GIVEN("A large virtual memory")
  {
    MEMORY_CONTROLLER dram{champsim::modules::ModuleBuilder{"t802_dram", "DEFAULT_MEMORY_CONTROLLER", champsim::defaults::default_memory_controller()}};
    VirtualMemory uut{champsim::modules::ModuleBuilder{"t802_uut", "DEFAULT_VMEM", champsim::defaults::default_vmem()}
        .add_parameter("page_table_page_size", pte_page_size)
        .add_parameter("dram", static_cast<champsim::modules::memory_controller_module*>(&dram))};

    champsim::data::bytes dist{1};
    for (std::size_t i = 0; i < level; ++i)
      dist *= pte_page_size.count();
    std::vector<champsim::page_number> req_pages{};
    req_pages.push_back(champsim::page_number{0xcccc000000000});
    const auto page_size = champsim::modules::ModuleBuilder::globals().get_parameter<unsigned>("page_size");
    for (auto i = pte_page_size; i < champsim::data::bytes{page_size}; i += pte_page_size)
      req_pages.push_back(req_pages.back() + dist);

    WHEN("A full set of requests for PTE entries at level " + std::to_string(level) + " are called for")
    {
      std::vector<champsim::address> given_pages{};
      std::transform(std::cbegin(req_pages), std::cend(req_pages), std::back_inserter(given_pages),
                     [&](auto req_page) { return uut.get_pte_pa(champsim::origin{0, 0}, req_page, level).first; });
      std::sort(std::begin(given_pages), std::end(given_pages));

      THEN("All entries are on the same page")
      {
        std::vector<champsim::page_number> pages;
        std::transform(std::cbegin(given_pages), std::cend(given_pages), std::back_inserter(pages), [](auto x) { return champsim::page_number{x}; });
        REQUIRE_THAT(pages, champsim::test::StrideMatcher<champsim::page_number>{0});
      }

      THEN("The entries are spaced by pte_page_size") { REQUIRE_THAT(given_pages, champsim::test::StrideMatcher<champsim::address>{pte_page_size.count()}); }
    }
  }
}
