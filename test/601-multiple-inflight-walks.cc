#include "catch.hpp"
#include "mocks.hpp"

#include "champsim_constants.h"
#include "dram_controller.h"
#include "ptw.h"
#include "vmem.h"

#include <array>

SCENARIO("A page table walker can handle multiple concurrent walks") {
  GIVEN("A 5-level virtual memory") {
    constexpr std::size_t levels = 5;
    MEMORY_CONTROLLER dram{1, 3200, 12.5, 12.5, 12.5, 7.5, {}};
    VirtualMemory vmem{1<<12, levels, 200, dram};
    do_nothing_MRC mock_ll{5};
    to_rq_MRP mock_ul;
    PageTableWalker uut{"600-uut-0", 0, 1, {{1,1}, {1,1}, {1,1}, {1,1}}, 2, 2, 1, 1, 1, {&mock_ul.queues}, &mock_ll.queues, vmem};

    std::array<champsim::operable*, 3> elements{{&mock_ul, &uut, &mock_ll}};

    uut.warmup = false;
    uut.begin_phase();

    WHEN("The PTW receives two requests") {
      PACKET test_a;
      test_a.address = 0xdeadbeefdeadbeef;
      test_a.v_address = test_a.address;
      test_a.cpu = 0;
      test_a.to_return = {&mock_ul.returned};

      PACKET test_b;
      test_b.address = 0xcafebabecafebabe;
      test_b.v_address = test_b.address;
      test_b.cpu = 0;
      test_b.to_return = {&mock_ul.returned};

      auto test_a_result = mock_ul.issue(test_a);
      REQUIRE(test_a_result);

      for (auto elem : elements)
        elem->_operate();

      auto test_b_result = mock_ul.issue(test_b);
      REQUIRE(test_b_result);

      for (auto i = 0; i < 10000; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("10 requests are issued") {
        REQUIRE(mock_ll.packet_count() == 2*levels);
        REQUIRE(mock_ul.packets.back().return_time > 0);
      }
    }
  }
}

