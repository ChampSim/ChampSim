#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"

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
    PageTableWalker uut{PageTableWalker::Builder{champsim::defaults::default_ptw}
      .name("601-uut")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .virtual_memory(&vmem)
      .tag_bandwidth(2)
      .fill_bandwidth(2)
    };

    std::array<champsim::operable*, 3> elements{{&mock_ul, &uut, &mock_ll}};

    uut.warmup = false;
    uut.begin_phase();

    WHEN("The PTW receives two requests") {
      decltype(mock_ul)::request_type test_a;
      test_a.address = 0xdeadbeefdeadbeef;
      test_a.v_address = test_a.address;
      test_a.cpu = 0;

      decltype(mock_ul)::request_type test_b;
      test_b.address = 0xcafebabecafebabe;
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
    constexpr uint64_t seed_address = 0xffff'ffff'ffff'ffff;
    constexpr uint64_t base_address = seed_address;
    constexpr uint64_t nearby_address = 0xffff'ffff'ffff'efff;

    MEMORY_CONTROLLER dram{1, 3200, 12.5, 12.5, 12.5, 7.5, {}};
    VirtualMemory vmem{1<<12, levels, 10, dram};
    release_MRC mock_ll;
    to_rq_MRP mock_ul{[](auto x, auto y){ return (x.address >> LOG2_BLOCK_SIZE) == (y.address >> LOG2_BLOCK_SIZE); }};
    //PageTableWalker uut{"601-uut-1", 0, 1, {{1,1}, {1,1}, {1,1}, {0,0}}, 1, 1, 1, 1, 1, {&mock_ul.queues}, &mock_ll.queues, vmem};
    PageTableWalker uut{PageTableWalker::Builder{champsim::defaults::default_ptw}
      .name("601-uut")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .virtual_memory(&vmem)
      .tag_bandwidth(2)
      .fill_bandwidth(2)
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
        CHECK(uut.pscl.at(0).check_hit({seed.address, 0, 4}).has_value());
        CHECK(uut.pscl.at(1).check_hit({seed.address, 0, 3}).has_value());
        CHECK(uut.pscl.at(2).check_hit({seed.address, 0, 2}).has_value());
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
            uint64_t nearby_paddr = 0x103ff8; // Hard coded to get the test to work...

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

