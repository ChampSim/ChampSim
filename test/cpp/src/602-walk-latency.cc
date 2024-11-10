#include "catch.hpp"
#include "mocks.hpp"

#include "defaults.hpp"
#include "dram_controller.h"
#include "ptw.h"
#include "vmem.h"

SCENARIO("The issued steps incur appropriate latencies") {
  auto level = GENERATE(1u,2u,3u,4u,5u);

  GIVEN("A 5-level virtual memory primed for "+std::to_string(level)+" accesses") {
    constexpr std::size_t vmem_levels = 5;
    champsim::address access_address{0xdeadbeef};
    constexpr std::chrono::nanoseconds penalty{640};
    MEMORY_CONTROLLER dram{champsim::chrono::picoseconds{3200}, champsim::chrono::picoseconds{6400}, std::size_t{18}, std::size_t{18}, std::size_t{18}, std::size_t{38}, champsim::chrono::microseconds{64000}, {}, 64, 64, 1, champsim::data::bytes{8}, 1024, 1024, 4, 4, 4, 8192};
    VirtualMemory vmem{champsim::data::bytes{1<<12}, vmem_levels, penalty, dram};
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    PageTableWalker uut{champsim::ptw_builder{champsim::defaults::default_ptw}
      .name("602-uut")
      .clock_period(champsim::chrono::picoseconds{3200})
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .virtual_memory(&vmem)
    };

    std::array<champsim::operable*, 3> elements{{&mock_ul, &uut, &mock_ll}};

    uut.warmup = false;
    uut.begin_phase();

    if (level == 5) {
      (void)vmem.va_to_pa(0, champsim::page_number{access_address});
      for (unsigned i = 0; i < 4; ++i)
        (void)vmem.get_pte_pa(0, champsim::page_number{access_address}, i);
    } else {
      for (unsigned i = 0; i < level; ++i)
        (void)vmem.get_pte_pa(0, champsim::page_number{access_address}, i);
    }

    WHEN("The PTW receives a request") {
      decltype(mock_ul)::request_type test;
      test.address = access_address;
      test.v_address = test.address;
      test.cpu = 0;

      auto test_result = mock_ul.issue(test);
      REQUIRE(test_result);

      for (auto i = 0; i < 10000; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The "+std::to_string(level-1)+"th packet responds to the delays imposed") {
        REQUIRE_THAT(mock_ul.packets.back(), champsim::test::ReturnedMatcher(200*static_cast<int>(vmem_levels-level+1) + 6, 1));
      }
    }
  }
}
