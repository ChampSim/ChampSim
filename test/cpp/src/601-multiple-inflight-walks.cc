#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"

#include "dram_controller.h"
#include "ptw.h"
#include "vmem.h"

#include <array>

SCENARIO("A page table walker can handle multiple concurrent walks") {
  GIVEN("A 5-level virtual memory") {
    constexpr std::size_t levels = 5;
    MEMORY_CONTROLLER dram{champsim::chrono::picoseconds{3200}, champsim::chrono::picoseconds{6400}, std::size_t{18}, std::size_t{18}, std::size_t{18}, std::size_t{38}, champsim::chrono::microseconds{64000}, {}, 64, 64, 1, champsim::data::bytes{8}, 1024, 1024, 4, 4, 4, 8192};
    VirtualMemory vmem{champsim::data::bytes{1<<12}, levels, champsim::chrono::nanoseconds{640}, dram};
    do_nothing_MRC mock_ll{5};
    to_rq_MRP mock_ul;
    PageTableWalker uut{champsim::ptw_builder{champsim::defaults::default_ptw}
      .name("601-uut")
      .clock_period(champsim::chrono::picoseconds{3200})
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .virtual_memory(&vmem)
      .tag_bandwidth(champsim::bandwidth::maximum_type{2})
      .fill_bandwidth(champsim::bandwidth::maximum_type{2})
    };

    std::array<champsim::operable*, 3> elements{{&mock_ul, &uut, &mock_ll}};

    uut.warmup = false;
    uut.begin_phase();

    WHEN("The PTW receives two requests") {
      decltype(mock_ul)::request_type test_a;
      test_a.address = champsim::address{0xdeadbeefdeadbeef};
      test_a.v_address = test_a.address;
      test_a.cpu = 0;

      decltype(mock_ul)::request_type test_b;
      test_b.address = champsim::address{0xcafebabecafebabe};
      test_b.v_address = test_b.address;
      test_b.cpu = 0;

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

SCENARIO("Concurrent page table walks can be merged") {
  GIVEN("A 5-level virtual memory") {
    constexpr std::size_t levels = 5;
    const champsim::address seed_address{0xffff'ffff'ffff'ffff};
    const champsim::address base_address{seed_address};
    const champsim::address nearby_address{0xffff'ffff'ffff'efff};

    MEMORY_CONTROLLER dram{champsim::chrono::picoseconds{3200}, champsim::chrono::picoseconds{6400}, std::size_t{18}, std::size_t{18}, std::size_t{18}, std::size_t{38}, champsim::chrono::microseconds{64000}, {}, 64, 64, 1, champsim::data::bytes{8}, 1024, 1024, 4, 4, 4, 8192};
    VirtualMemory vmem{champsim::data::bytes{1<<12}, levels, champsim::chrono::nanoseconds{10}, dram};
    release_MRC mock_ll;
    to_rq_MRP mock_ul{[](auto x, auto y){ return champsim::block_number{x.address} == champsim::block_number{y.address}; }};
    PageTableWalker uut{champsim::ptw_builder{champsim::defaults::default_ptw}
      .name("601-uut")
      .clock_period(champsim::chrono::picoseconds{3200})
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .virtual_memory(&vmem)
      .tag_bandwidth(champsim::bandwidth::maximum_type{2})
      .fill_bandwidth(champsim::bandwidth::maximum_type{2})
    };

    std::array<champsim::operable*, 3> elements{{&mock_ul, &uut, &mock_ll}};

    uut.warmup = false;
    uut.begin_phase();

    WHEN("The PTW receives a request and fills the PSCLs") {
      decltype(mock_ul)::request_type seed;
      seed.address = seed_address;
      seed.v_address = seed.address;
      seed.cpu = 0;

      auto seed_result = mock_ul.issue(seed);
      THEN("The issue is accepted") {
        REQUIRE(seed_result);
      }

      for (auto j = 0; j < 5; ++j) {
        for (auto i = 0; i < 100; ++i) {
          for (auto elem : elements)
            elem->_operate();
        }
        mock_ll.release_all();
      }

      THEN("The PSCLs contain the request's address") {
        CHECK(uut.pscl.at(0).check_hit(typename decltype(PageTableWalker::pscl)::value_type::value_type{seed.address, champsim::address{}, 4}).has_value());
        CHECK(uut.pscl.at(1).check_hit(typename decltype(PageTableWalker::pscl)::value_type::value_type{seed.address, champsim::address{}, 3}).has_value());
        CHECK(uut.pscl.at(2).check_hit(typename decltype(PageTableWalker::pscl)::value_type::value_type{seed.address, champsim::address{}, 2}).has_value());
      }

      AND_WHEN("The PTW receives a request") {
        decltype(mock_ul)::request_type test;
        test.address = base_address;
        test.v_address = test.address;
        test.cpu = 0;

        auto test_result = mock_ul.issue(test);
        THEN("The issue is accepted") {
          REQUIRE(test_result);
        }

        for (auto i = 0; i < 100; ++i) {
          for (auto elem : elements)
            elem->_operate();
        }

        AND_WHEN("The PTW receives a nearby request before the first returns") {
          decltype(mock_ul)::request_type nearby;
          nearby.address = nearby_address;
          nearby.v_address = nearby.address;
          nearby.cpu = 0;

          auto nearby_result = mock_ul.issue(nearby);
          THEN("The issue is accepted") {
            REQUIRE(nearby_result);
          }

          for (auto i = 0; i < 100; ++i) {
            for (auto elem : elements)
              elem->_operate();
          }

          AND_WHEN("The lower level returns") {
            champsim::address nearby_paddr{0x103ff8}; // Hard coded to get the test to work...

            mock_ll.release(nearby_paddr);

            for (auto j = 0; j < 3; ++j) {
              for (auto i = 0; i < 100; ++i) {
                for (auto elem : elements)
                  elem->_operate();
              }
              mock_ll.release_all();
            }

            THEN("Both paths are completed") {
              CHECK(mock_ul.packets.at(2).return_time > 0);
              CHECK(mock_ul.packets.at(1).return_time > 0);
            }
          }
        }
      }
    }
  }
}

