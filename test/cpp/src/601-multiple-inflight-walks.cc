#include <array>
#include <catch.hpp>

#include "defaults.hpp"
#include "dram_controller.h"
#include "mocks.hpp"
#include "ptw.h"
#include "vmem.h"

SCENARIO("A page table walker can handle multiple concurrent walks")
{
  GIVEN("A 5-level virtual memory")
  {
    constexpr std::size_t levels = 5;
    MEMORY_CONTROLLER dram{champsim::modules::ModuleBuilder{"t601_dram_0", "DEFAULT_MEMORY_CONTROLLER", champsim::defaults::default_memory_controller()}};
    VirtualMemory vmem{champsim::modules::ModuleBuilder{"t601_vmem_0", "DEFAULT_VMEM", champsim::defaults::default_vmem()}
        .add_parameter("page_table_levels", static_cast<std::size_t>(levels))
        .add_parameter("minor_fault_penalty", champsim::chrono::picoseconds{champsim::chrono::nanoseconds{640}})
        .add_parameter("dram", static_cast<champsim::modules::memory_controller_module*>(&dram))};
    do_nothing_MRC mock_ll{5};
    to_rq_MRP mock_ul;
    PageTableWalker uut{champsim::modules::ModuleBuilder{"t601_ptw_0", "DEFAULT_PTW", champsim::defaults::default_ptw()}
                            .add_parameter("clock_period", champsim::chrono::picoseconds{3200})
                            .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                            .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                            .add_parameter("vmem", static_cast<champsim::modules::vmem_module*>(&vmem))
                            .add_parameter("max_tag_check", champsim::bandwidth::maximum_type{2})
                            .add_parameter("max_fill", champsim::bandwidth::maximum_type{2})};

    std::array<champsim::operable*, 3> elements{{&mock_ul, &uut, &mock_ll}};

    uut.begin_phase(false);

    WHEN("The PTW receives two requests")
    {
      decltype(mock_ul)::request_type test_a;
      test_a.address = champsim::address{0xdeadbeefdeadbeef};
      test_a.v_address = test_a.address;
      test_a.origin = champsim::origin{0, 0};

      decltype(mock_ul)::request_type test_b;
      test_b.address = champsim::address{0xcafebabecafebabe};
      test_b.v_address = test_b.address;
      test_b.origin = champsim::origin{0, 0};

      auto test_a_result = mock_ul.issue(test_a);
      REQUIRE(test_a_result);

      for (auto elem : elements)
        elem->_operate();

      auto test_b_result = mock_ul.issue(test_b);
      REQUIRE(test_b_result);

      for (auto i = 0; i < 10000; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("10 requests are issued")
      {
        REQUIRE(mock_ll.packet_count() == 2 * levels);
        REQUIRE(mock_ul.packets.back().return_time > 0);
      }
    }
  }
}

SCENARIO("Concurrent page table walks can be merged")
{
  GIVEN("A 5-level virtual memory")
  {
    constexpr std::size_t levels = 5;
    const champsim::address seed_address{0xffff'ffff'ffff'ffff};
    const champsim::address base_address{seed_address};
    const champsim::address nearby_address{0xffff'ffff'ffff'efff};

    MEMORY_CONTROLLER dram{champsim::modules::ModuleBuilder{"t601_dram_1", "DEFAULT_MEMORY_CONTROLLER", champsim::defaults::default_memory_controller()}};
    VirtualMemory vmem{champsim::modules::ModuleBuilder{"t601_vmem_1", "DEFAULT_VMEM", champsim::defaults::default_vmem()}
        .add_parameter("page_table_levels", static_cast<std::size_t>(levels))
        .add_parameter("minor_fault_penalty", champsim::chrono::picoseconds{champsim::chrono::nanoseconds{10}})
        .add_parameter("dram", static_cast<champsim::modules::memory_controller_module*>(&dram))};
    release_MRC mock_ll;
    to_rq_MRP mock_ul{[](auto x, auto y) {
      return champsim::block_number{x.address} == champsim::block_number{y.address};
    }};
    PageTableWalker uut{champsim::modules::ModuleBuilder{"t601_ptw_1", "DEFAULT_PTW", champsim::defaults::default_ptw()}
                            .add_parameter("clock_period", champsim::chrono::picoseconds{3200})
                            .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                            .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                            .add_parameter("vmem", static_cast<champsim::modules::vmem_module*>(&vmem))
                            .add_parameter("max_tag_check", champsim::bandwidth::maximum_type{2})
                            .add_parameter("max_fill", champsim::bandwidth::maximum_type{2})};

    std::array<champsim::operable*, 3> elements{{&mock_ul, &uut, &mock_ll}};

    uut.begin_phase(false);

    WHEN("The PTW receives a request and fills the PSCLs")
    {
      decltype(mock_ul)::request_type seed;
      seed.address = seed_address;
      seed.v_address = seed.address;
      seed.origin = champsim::origin{0, 0};

      auto seed_result = mock_ul.issue(seed);
      THEN("The issue is accepted") { REQUIRE(seed_result); }

      for (auto j = 0; j < 5; ++j) {
        for (auto i = 0; i < 100; ++i) {
          for (auto elem : elements)
            elem->_operate();
        }
        mock_ll.release_all();
      }

      THEN("The PSCLs contain the request's address")
      {
        CHECK(uut.pscl.at(0).check_hit(typename decltype(PageTableWalker::pscl)::value_type::value_type{seed.address, champsim::address{}, 4}).has_value());
        CHECK(uut.pscl.at(1).check_hit(typename decltype(PageTableWalker::pscl)::value_type::value_type{seed.address, champsim::address{}, 3}).has_value());
        CHECK(uut.pscl.at(2).check_hit(typename decltype(PageTableWalker::pscl)::value_type::value_type{seed.address, champsim::address{}, 2}).has_value());
      }

      AND_WHEN("The PTW receives a request")
      {
        decltype(mock_ul)::request_type test;
        test.address = base_address;
        test.v_address = test.address;
        test.origin = champsim::origin{0, 0};

        auto test_result = mock_ul.issue(test);
        THEN("The issue is accepted") { REQUIRE(test_result); }

        for (auto i = 0; i < 100; ++i) {
          for (auto elem : elements)
            elem->_operate();
        }

        AND_WHEN("The PTW receives a nearby request before the first returns")
        {
          decltype(mock_ul)::request_type nearby;
          nearby.address = nearby_address;
          nearby.v_address = nearby.address;
          nearby.origin = champsim::origin{0, 0};

          auto nearby_result = mock_ul.issue(nearby);
          THEN("The issue is accepted") { REQUIRE(nearby_result); }

          for (auto i = 0; i < 100; ++i) {
            for (auto elem : elements)
              elem->_operate();
          }

          AND_WHEN("The lower level returns")
          {
            champsim::address nearby_paddr{0x103ff8}; // Hard coded to get the test to work...

            mock_ll.release(nearby_paddr);

            for (auto j = 0; j < 3; ++j) {
              for (auto i = 0; i < 100; ++i) {
                for (auto elem : elements)
                  elem->_operate();
              }
              mock_ll.release_all();
            }

            THEN("Both paths are completed")
            {
              CHECK(mock_ul.packets.at(2).return_time > 0);
              CHECK(mock_ul.packets.at(1).return_time > 0);
            }
          }
        }
      }
    }
  }
}
