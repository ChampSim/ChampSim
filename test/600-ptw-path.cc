#include "catch.hpp"
#include "mocks.hpp"

#include "champsim_constants.h"
#include "ptw.h"
#include "vmem.h"

#include <array>

SCENARIO("The number of issued steps matches the virtual memory levels") {
  GIVEN("A 5-level virtual memory") {
    constexpr std::size_t levels = 5;
    MEMORY_CONTROLLER dram{1, 3200, 12.5, 12.5, 12.5, 7.5};
    VirtualMemory vmem{20, 1<<12, levels, 200, dram};
    do_nothing_MRC mock_ll;
    PageTableWalker uut{"600-uut-0", 0, 1, {{1,1}, {1,1}, {1,1}, {1,1}}, 1, 1, 1, 1, 1, &mock_ll, vmem};
    to_rq_MRP mock_ul{&uut};

    std::array<champsim::operable*, 3> elements{{&mock_ul, &uut, &mock_ll}};

    uut.warmup = false;
    uut.begin_phase();

    WHEN("The PTW receives a request") {
      PACKET test;
      test.instr_id = 0;
      test.address = 0xdeadbeef;
      test.v_address = test.address;
      test.asid = 0;
      test.to_return = {&mock_ul};

      auto test_result = mock_ul.issue(test);
      REQUIRE(test_result);

      for (auto i = 0; i < 10000; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("5 requests are issued") {
        REQUIRE(mock_ll.packet_count() == levels);
        REQUIRE(mock_ul.packets.back().return_time > 0);
      }
    }
  }
}

SCENARIO("Issuing a PTW fills the PSCLs") {
  GIVEN("A 5-level virtual memory") {
    constexpr std::size_t levels = 5;
    MEMORY_CONTROLLER dram{1, 3200, 12.5, 12.5, 12.5, 7.5};
    VirtualMemory vmem{33, 1<<12, levels, 200, dram};
    do_nothing_MRC mock_ll;
    PageTableWalker uut{"600-uut-1", 0, 1, {{1,1}, {1,1}, {1,1}, {1,1}}, 1, 1, 1, 1, 1, &mock_ll, vmem};
    to_rq_MRP mock_ul{&uut};

    std::array<champsim::operable*, 3> elements{{&mock_ul, &uut, &mock_ll}};

    uut.warmup = false;
    uut.begin_phase();

    WHEN("The PTW receives a request") {
      PACKET test;
      test.instr_id = 0;
      test.address = 0xffff'ffff'ffff'ffff;
      test.v_address = test.address;
      test.asid = 0;
      test.to_return = {&mock_ul};

      auto test_result = mock_ul.issue(test);
      REQUIRE(test_result);

      for (auto i = 0; i < 10000; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The PSCLs contain the request's address") {
        CHECK(uut.pscl.at(0).check_hit({test.asid, test.v_address, 0, 4}).has_value());
        CHECK(uut.pscl.at(1).check_hit({test.asid, test.v_address, 0, 3}).has_value());
        CHECK(uut.pscl.at(2).check_hit({test.asid, test.v_address, 0, 2}).has_value());
        CHECK(uut.pscl.at(3).check_hit({test.asid, test.v_address, 0, 1}).has_value());
      }
    }
  }
}

struct pscl_testbench
{
  constexpr static std::size_t levels = 5;
  constexpr static uint64_t seed_addr = 0xffff'ffff'ffff'ffff;
  MEMORY_CONTROLLER dram{1, 3200, 12.5, 12.5, 12.5, 7.5};
  VirtualMemory vmem{33, 1<<12, levels, 200, dram};
  do_nothing_MRC mock_ll;
  PageTableWalker uut{"600-uut-2", 0, 1, {{1,1}, {1,1}, {1,1}, {1,1}}, 1, 1, 1, 1, 1, &mock_ll, vmem};
  to_rq_MRP mock_ul{&uut};

  std::array<champsim::operable*, 3> elements{{&mock_ul, &uut, &mock_ll}};

  PACKET seed;
  uint16_t asid;
  uint64_t id = 0;

  pscl_testbench(uint16_t asid) : asid(asid) {
    uut.warmup = false;
    uut.begin_phase();

    seed.instr_id = id++;
    seed.address = seed_addr;
    seed.v_address = seed.address;
    seed.asid = asid;
    seed.to_return = {&mock_ul};

    auto seed_result = mock_ul.issue(seed);
    REQUIRE(seed_result);

    for (auto i = 0; i < 10000; ++i)
      for (auto elem : elements)
        elem->_operate();
  }

  bool issue(uint64_t addr)
  {
    mock_ll.addresses.clear();

    PACKET test = seed;
    test.instr_id = id++;
    test.address = addr;
    test.v_address = test.address;

    auto test_result = mock_ul.issue(test);

    for (auto i = 0; i < 10000; ++i)
      for (auto elem : elements)
        elem->_operate();

    return test_result;
  }
};

SCENARIO("PSCLs eliminate 4 requests on identical requests") {
  GIVEN("A 5-level virtual memory and one issued packet") {
    pscl_testbench testbench{1};

    WHEN("The PTW receives the same request") {
      auto test_result = testbench.issue(testbench.seed_addr);
      REQUIRE(test_result);

      THEN("1 request is issued") {
        REQUIRE(testbench.mock_ll.packet_count() == 1);
        REQUIRE(testbench.mock_ul.packets.back().return_time > 0);
      }
    }
  }
}

SCENARIO("PSCLs eliminate 3 requests on nearby requests") {
  GIVEN("A 5-level virtual memory and one issued packet") {
    pscl_testbench testbench{2};

    WHEN("The PTW receives a nearby request") {
      auto test_result = testbench.issue(0xffff'ffff'ffc0'0000);
      REQUIRE(test_result);

      THEN("2 requests are issued") {
        REQUIRE(testbench.mock_ll.packet_count() == 2);
        REQUIRE(testbench.mock_ul.packets.back().return_time > 0);
      }
    }
  }
}

SCENARIO("PSCLs eliminate 3 requests on less-nearby requests") {
  GIVEN("A 5-level virtual memory and one issued packet") {
    pscl_testbench testbench{3};

    WHEN("The PTW receives a less-nearby request") {
      auto test_result = testbench.issue(0xffff'ffff'8000'0000);
      REQUIRE(test_result);

      THEN("3 requests are issued") {
        REQUIRE(testbench.mock_ll.packet_count() == 3);
        REQUIRE(testbench.mock_ul.packets.back().return_time > 0);
      }
    }
  }
}

SCENARIO("PSCLs eliminate 1 request on distant requests") {
  GIVEN("A 5-level virtual memory and one issued packet") {
    pscl_testbench testbench{4};

    WHEN("The PTW receives a distant request") {
      auto test_result = testbench.issue(0xffff'ff00'0000'0000);
      REQUIRE(test_result);

      THEN("4 requests are issued") {
        REQUIRE(testbench.mock_ll.packet_count() == 4);
        REQUIRE(testbench.mock_ul.packets.back().return_time > 0);
      }
    }
  }
}

SCENARIO("PSCLs can reduce the number of issued translation requests") {
  GIVEN("A 5-level virtual memory and one issued packet") {
    pscl_testbench testbench{5};

    WHEN("The PTW receives a very distant request") {
      auto test_result = testbench.issue(0xfffe'0000'0000'0000);
      REQUIRE(test_result);

      THEN("5 requests are issued") {
        REQUIRE(testbench.mock_ll.packet_count() == 5);
        REQUIRE(testbench.mock_ul.packets.back().return_time > 0);
      }
    }
  }
}
