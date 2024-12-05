#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"

#include "dram_controller.h"
#include "ptw.h"
#include "vmem.h"

#include <array>

SCENARIO("The number of issued steps matches the virtual memory levels") {
  GIVEN("A 5-level virtual memory") {
    constexpr std::size_t levels = 5;
    MEMORY_CONTROLLER dram{champsim::chrono::picoseconds{3200}, champsim::chrono::picoseconds{6400}, std::size_t{18}, std::size_t{18}, std::size_t{18}, std::size_t{38}, champsim::chrono::microseconds{64000}, {}, 64, 64, 1, champsim::data::bytes{8}, 1024, 1024, 4, 4, 4, 8192};
    VirtualMemory vmem{champsim::data::bytes{1<<12}, levels, champsim::chrono::nanoseconds{640}, dram};
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    PageTableWalker uut{champsim::ptw_builder{champsim::defaults::default_ptw}
      .name("600a-uut")
      .clock_period(champsim::chrono::picoseconds{3200})
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .virtual_memory(&vmem)
    };

    std::array<champsim::operable*, 3> elements{{&mock_ul, &uut, &mock_ll}};

    uut.warmup = false;
    uut.begin_phase();

    WHEN("The PTW receives a request") {
      decltype(mock_ul)::request_type test;
      test.address = champsim::address{0xdeadbeef};
      test.v_address = test.address;
      test.cpu = 0;

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
    MEMORY_CONTROLLER dram{champsim::chrono::picoseconds{3200}, champsim::chrono::picoseconds{6400}, std::size_t{18}, std::size_t{18}, std::size_t{18}, std::size_t{38}, champsim::chrono::microseconds{64000}, {}, 64, 64, 1, champsim::data::bytes{8}, 1024, 1024, 4, 4, 4, 8192};
    VirtualMemory vmem{champsim::data::bytes{1<<12}, levels, champsim::chrono::nanoseconds{640}, dram};
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    PageTableWalker uut{champsim::ptw_builder{champsim::defaults::default_ptw}
      .name("600b-uut")
      .clock_period(champsim::chrono::picoseconds{3200})
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .virtual_memory(&vmem)
      .add_pscl(5,1,1)
      .add_pscl(4,1,1)
      .add_pscl(3,1,1)
      .add_pscl(2,1,1)
    };

    std::array<champsim::operable*, 3> elements{{&mock_ul, &uut, &mock_ll}};

    uut.warmup = false;
    uut.begin_phase();

    WHEN("The PTW receives a request") {
      decltype(mock_ul)::request_type test;
      test.address = champsim::address{0xffff'ffff'ffff'ffff};
      test.v_address = test.address;
      test.cpu = 0;

      auto test_result = mock_ul.issue(test);
      REQUIRE(test_result);

      for (auto i = 0; i < 10000; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The PSCLs contain the request's address") {
        CHECK(uut.pscl.at(0).check_hit({test.address, champsim::address{}, 4}).has_value());
        CHECK(uut.pscl.at(1).check_hit({test.address, champsim::address{}, 3}).has_value());
        CHECK(uut.pscl.at(2).check_hit({test.address, champsim::address{}, 2}).has_value());
        CHECK(uut.pscl.at(3).check_hit({test.address, champsim::address{}, 1}).has_value());
      }
    }
  }
}

SCENARIO("PSCLs can reduce the number of issued translation requests") {
  GIVEN("A 5-level virtual memory and one issued packet") {
    constexpr std::size_t levels = 5;
    MEMORY_CONTROLLER dram{champsim::chrono::picoseconds{3200}, champsim::chrono::picoseconds{6400}, std::size_t{18}, std::size_t{18}, std::size_t{18}, std::size_t{38}, champsim::chrono::microseconds{64000}, {}, 64, 64, 1, champsim::data::bytes{8}, 1024, 1024, 4, 4, 4, 8192};
    VirtualMemory vmem{champsim::data::bytes{1<<12}, levels, champsim::chrono::nanoseconds{640}, dram};
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    PageTableWalker uut{champsim::ptw_builder{champsim::defaults::default_ptw}
      .name("600c-uut")
      .clock_period(champsim::chrono::picoseconds{3200})
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .virtual_memory(&vmem)
      .add_pscl(5,1,1)
      .add_pscl(4,1,1)
      .add_pscl(3,1,1)
      .add_pscl(2,1,1)
    };

    std::array<champsim::operable*, 3> elements{{&mock_ul, &uut, &mock_ll}};

    uut.warmup = false;
    uut.begin_phase();

    decltype(mock_ul)::request_type seed;
    seed.address = champsim::address{0xffff'ffff'ffff'ffff};
    seed.v_address = seed.address;
    seed.cpu = 0;

    auto seed_result = mock_ul.issue(seed);
    REQUIRE(seed_result);

    for (auto i = 0; i < 10000; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("The PTW receives the same request") {
      mock_ll.addresses.clear();

      auto test_result = mock_ul.issue(seed);
      REQUIRE(test_result);

      for (auto i = 0; i < 10000; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("1 request is issued") {
        REQUIRE(mock_ll.packet_count() == 1);
        REQUIRE(mock_ul.packets.back().return_time > 0);
      }
    }

    WHEN("The PTW receives a nearby request") {
      mock_ll.addresses.clear();

      decltype(mock_ul)::request_type test = seed;
      test.address = champsim::address{0xffff'ffff'ffc0'0000};
      test.v_address = test.address;
      auto test_result = mock_ul.issue(test);
      REQUIRE(test_result);

      for (auto i = 0; i < 10000; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("2 requests are issued") {
        REQUIRE(mock_ll.packet_count() == 2);
        REQUIRE(mock_ul.packets.back().return_time > 0);
      }
    }

    WHEN("The PTW receives a less-nearby request") {
      mock_ll.addresses.clear();

      decltype(mock_ul)::request_type test = seed;
      test.address = champsim::address{0xffff'ffff'8000'0000};
      test.v_address = test.address;
      auto test_result = mock_ul.issue(test);
      REQUIRE(test_result);

      for (auto i = 0; i < 10000; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("3 requests are issued") {
        REQUIRE(mock_ll.packet_count() == 3);
        REQUIRE(mock_ul.packets.back().return_time > 0);
      }
    }

    WHEN("The PTW receives a distant request") {
      mock_ll.addresses.clear();

      decltype(mock_ul)::request_type test = seed;
      test.address = champsim::address{0xffff'ff00'0000'0000};
      test.v_address = test.address;
      auto test_result = mock_ul.issue(test);
      REQUIRE(test_result);

      for (auto i = 0; i < 10000; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("4 requests are issued") {
        REQUIRE(mock_ll.packet_count() == 4);
        REQUIRE(mock_ul.packets.back().return_time > 0);
      }
    }

    WHEN("The PTW receives a very distant request") {
      mock_ll.addresses.clear();

      decltype(mock_ul)::request_type test = seed;
      test.address = champsim::address{0xfffe'0000'0000'0000};
      test.v_address = test.address;
      auto test_result = mock_ul.issue(test);
      REQUIRE(test_result);

      for (auto i = 0; i < 10000; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("5 requests are issued") {
        REQUIRE(mock_ll.packet_count() == 5);
        REQUIRE(mock_ul.packets.back().return_time > 0);
      }
    }
  }
}
