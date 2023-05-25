#include "catch.hpp"
#include "mocks.hpp"

#include "champsim_constants.h"
#include "defaults.hpp"
#include "dram_controller.h"
#include "ptw.h"
#include "vmem.h"

SCENARIO("The issued steps incur appropriate latencies") {
  auto level = GENERATE(1u,2u,3u,4u,5u);

  GIVEN("A 5-level virtual memory primed for "+std::to_string(level)+" accesses") {
    constexpr std::size_t vmem_levels = 5;
    constexpr uint64_t access_address = 0xdeadbeef;
    constexpr uint64_t penalty = 200;
    MEMORY_CONTROLLER dram{1, 3200, 12.5, 12.5, 12.5, 7.5, {}};
    VirtualMemory vmem{1<<12, vmem_levels, penalty, dram};
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    PageTableWalker uut{PageTableWalker::Builder{champsim::defaults::default_ptw}
      .name("602-uut")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .virtual_memory(&vmem)
    };

    std::array<champsim::operable*, 3> elements{{&mock_ul, &uut, &mock_ll}};

    uut.warmup = false;
    uut.begin_phase();

    if (level == 5) {
      (void)vmem.va_to_pa(0, access_address);
      for (unsigned i = 0; i < 4; ++i)
        (void)vmem.get_pte_pa(0, access_address, i);
    } else {
      for (unsigned i = 0; i < level; ++i)
        (void)vmem.get_pte_pa(0, access_address, i);
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
        REQUIRE(mock_ul.packets.back().return_time == (penalty*(vmem_levels-level+1) + 6));
      }
    }
  }
}
