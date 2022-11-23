#include "catch.hpp"
#include "mocks.hpp"

#include "champsim_constants.h"
#include "ptw.h"
#include "vmem.h"

#include <array>

SCENARIO("The page table steps have correct offsets") {
  auto level = GENERATE(as<unsigned>{}, 1,2,3,4);
  GIVEN("A 5-level virtual memory") {
    constexpr std::size_t levels = 5;
    MEMORY_CONTROLLER dram{1, 3200, 12.5, 12.5, 12.5, 7.5};
    VirtualMemory vmem{30, 1<<12, levels, 200, dram};
    do_nothing_MRC mock_ll;
    PageTableWalker uut{"603-uut-"+std::to_string(level), 0, 1, {{1,1}, {1,1}, {1,1}, {1,1}}, 1, 1, 1, 1, 1, &mock_ll, vmem};
    to_rq_MRP mock_ul{&uut};

    std::array<champsim::operable*, 3> elements{{&mock_ul, &uut, &mock_ll}};

    uut.warmup = false;
    uut.begin_phase();

    //uint64_t addr = (0xffff'ffff'ffe0'0000 | ((3*(level+1)) << LOG2_PAGE_SIZE)) << (level * 9);
    uint64_t addr = 0x0040'0200'c040'1000; // 0x4, 0x3, 0x2, 0x1

    WHEN("The PTW receives a request") {
      PACKET test;
      test.address = addr;
      test.v_address = test.address;
      test.cpu = 0;
      test.to_return = {&mock_ul};

      auto test_result = mock_ul.issue(test);
      REQUIRE(test_result);

      for (auto i = 0; i < 10000; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The " + std::to_string(level) + "th request has the correct offset") {
        REQUIRE(mock_ll.packet_count() == levels);
        REQUIRE((mock_ll.addresses.at(levels-level) & champsim::bitmask(12)) == level * PTE_BYTES);
      }
    }
  }
}


