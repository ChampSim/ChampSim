#include <catch.hpp>

#include "defaults.hpp"
#include "dram_controller.h"
#include "mocks.hpp"
#include "ptw.h"
#include "vmem.h"

SCENARIO("The issued steps incur appropriate latencies")
{
  auto level = GENERATE(1u, 2u, 3u, 4u, 5u);

  GIVEN("A 5-level virtual memory primed for " + std::to_string(level) + " accesses")
  {
    constexpr std::size_t vmem_levels = 5;
    champsim::address access_address{0xdeadbeef};
    constexpr std::chrono::nanoseconds penalty{640};
    MEMORY_CONTROLLER dram{champsim::modules::ModuleBuilder{"t602_dram", "DEFAULT_MEMORY_CONTROLLER", champsim::defaults::default_memory_controller()}};
    VirtualMemory vmem{champsim::modules::ModuleBuilder{"t602_vmem", "DEFAULT_VMEM", champsim::defaults::default_vmem()}
        .add_parameter("page_table_levels", static_cast<std::size_t>(vmem_levels))
        .add_parameter("minor_fault_penalty", champsim::chrono::picoseconds{penalty})
        .add_parameter("dram", static_cast<champsim::modules::memory_controller_module*>(&dram))};
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    PageTableWalker uut{champsim::modules::ModuleBuilder{"t602_ptw", "DEFAULT_PTW", champsim::defaults::default_ptw()}
                            .add_parameter("clock_period", champsim::chrono::picoseconds{3200})
                            .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                            .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                            .add_parameter("vmem", static_cast<champsim::modules::vmem_module*>(&vmem))};

    std::array<champsim::operable*, 3> elements{{&mock_ul, &uut, &mock_ll}};

    uut.begin_phase(false);

    if (level == 5) {
      (void)vmem.va_to_pa(champsim::origin{0, 0}, champsim::page_number{access_address});
      for (unsigned i = 0; i < 4; ++i)
        (void)vmem.get_pte_pa(champsim::origin{0, 0}, champsim::page_number{access_address}, i);
    } else {
      for (unsigned i = 0; i < level; ++i)
        (void)vmem.get_pte_pa(champsim::origin{0, 0}, champsim::page_number{access_address}, i);
    }

    WHEN("The PTW receives a request")
    {
      decltype(mock_ul)::request_type test;
      test.address = access_address;
      test.v_address = test.address;
      test.origin = champsim::origin{0, 0};

      auto test_result = mock_ul.issue(test);
      REQUIRE(test_result);

      for (auto i = 0; i < 10000; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The " + std::to_string(level - 1) + "th packet responds to the delays imposed")
      {
        REQUIRE_THAT(mock_ul.packets.back(), champsim::test::ReturnedMatcher(200 * static_cast<int>(vmem_levels - level + 1) + 6, 1));
      }
    }
  }
}
