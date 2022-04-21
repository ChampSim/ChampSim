#include "catch.hpp"
#include "mocks.hpp"

#include "champsim_constants.h"
#include "ptw.h"
#include "vmem.h"

#include <array>

extern bool warmup_complete[NUM_CPUS];

SCENARIO("The number of issued steps matches the virtual memory levels") {
  GIVEN("A 5-level virtual memory") {
    constexpr std::size_t levels = 5;
    VirtualMemory vmem{20, 1<<12, levels, 1, 200};
    do_nothing_MRC mock_ll;
    PageTableWalker uut{"600-uut", 0, 0, {1,1,0}, {1,1,0}, {1,1,0}, {1,1,0}, 1, 1, 1, 1, 0, &mock_ll, vmem};
    to_rq_MRP mock_ul{&uut};

    std::array<champsim::operable*, 3> elements{{&mock_ul, &uut, &mock_ll}};

    std::fill(std::begin(warmup_complete), std::end(warmup_complete), false);

    WHEN("The PTW receives a request") {
      PACKET test;
      test.address = 0xdeadbeef;
      test.cpu = 0;
      test.to_return = {&mock_ul};

      auto test_result = mock_ul.issue(test);
      REQUIRE(test_result);

      for (auto i = 0; i < 10000; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("5 requests are issued") {
        REQUIRE(std::size(mock_ll.addresses) == levels);
        REQUIRE(mock_ul.packets.front().return_time > 0);
      }
    }
  }
}
