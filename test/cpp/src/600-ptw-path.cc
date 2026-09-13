#include <array>
#include <catch.hpp>

#include "defaults.hpp"
#include "dram_controller.h"
#include "mocks.hpp"
#include "ptw.h"
#include "vmem.h"

SCENARIO("The number of issued steps matches the virtual memory levels")
{
  GIVEN("A 5-level virtual memory")
  {
    constexpr std::size_t levels = 5;
    MEMORY_CONTROLLER dram{champsim::modules::ModuleBuilder{"t600_dram_0", "DEFAULT_MEMORY_CONTROLLER", champsim::defaults::default_memory_controller()}};
    VirtualMemory vmem{champsim::modules::ModuleBuilder{"t600_vmem_0", "DEFAULT_VMEM", champsim::defaults::default_vmem()}
        .add_parameter("dram", static_cast<champsim::modules::memory_controller_module*>(&dram))
        .add_parameter("page_table_levels", levels)
        .add_parameter("minor_fault_penalty", champsim::chrono::picoseconds{640000})};
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    PageTableWalker uut{champsim::modules::ModuleBuilder{"t600_ptw_0", "DEFAULT_PTW", champsim::defaults::default_ptw()}
                            .add_parameter("clock_period", champsim::chrono::picoseconds{3200})
                            .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                            .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                            .add_parameter("vmem", static_cast<champsim::modules::vmem_module*>(&vmem))};

    std::array<champsim::operable*, 3> elements{{&mock_ul, &uut, &mock_ll}};

    uut.begin_phase(false);

    WHEN("The PTW receives a request")
    {
      decltype(mock_ul)::request_type test;
      test.address = champsim::address{0xdeadbeef};
      test.v_address = test.address;
      test.origin = champsim::origin{0, 0};

      auto test_result = mock_ul.issue(test);
      REQUIRE(test_result);

      for (auto i = 0; i < 10000; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("5 requests are issued")
      {
        REQUIRE(mock_ll.packet_count() == levels);
        REQUIRE(mock_ul.packets.back().return_time > 0);
      }
    }
  }
}

SCENARIO("Issuing a PTW fills the PSCLs")
{
  GIVEN("A 5-level virtual memory")
  {
    constexpr std::size_t levels = 5;
    MEMORY_CONTROLLER dram{champsim::modules::ModuleBuilder{"t600_dram_1", "DEFAULT_MEMORY_CONTROLLER", champsim::defaults::default_memory_controller()}};
    VirtualMemory vmem{champsim::modules::ModuleBuilder{"t600_vmem_1", "DEFAULT_VMEM", champsim::defaults::default_vmem()}
        .add_parameter("dram", static_cast<champsim::modules::memory_controller_module*>(&dram))
        .add_parameter("page_table_levels", levels)
        .add_parameter("minor_fault_penalty", champsim::chrono::picoseconds{640000})};
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    PageTableWalker uut{champsim::modules::ModuleBuilder{"t600_ptw_1", "DEFAULT_PTW", champsim::defaults::default_ptw()}
                            .add_parameter("clock_period", champsim::chrono::picoseconds{3200})
                            .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                            .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                            .add_parameter("vmem", static_cast<champsim::modules::vmem_module*>(&vmem))
                            .add_parameter("pscl_dims", std::vector<std::array<uint32_t, 3>>{{5, 1, 1}, {4, 1, 1}, {3, 1, 1}, {2, 1, 1}})};

    std::array<champsim::operable*, 3> elements{{&mock_ul, &uut, &mock_ll}};

    uut.begin_phase(false);

    WHEN("The PTW receives a request")
    {
      decltype(mock_ul)::request_type test;
      test.address = champsim::address{0xffff'ffff'ffff'ffff};
      test.v_address = test.address;
      test.origin = champsim::origin{0, 0};

      auto test_result = mock_ul.issue(test);
      REQUIRE(test_result);

      for (auto i = 0; i < 10000; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The PSCLs contain the request's address")
      {
        CHECK(uut.pscl.at(0).check_hit({test.address, champsim::address{}, 4}).has_value());
        CHECK(uut.pscl.at(1).check_hit({test.address, champsim::address{}, 3}).has_value());
        CHECK(uut.pscl.at(2).check_hit({test.address, champsim::address{}, 2}).has_value());
        CHECK(uut.pscl.at(3).check_hit({test.address, champsim::address{}, 1}).has_value());
      }
    }
  }
}

SCENARIO("PSCLs can reduce the number of issued translation requests")
{
  GIVEN("A 5-level virtual memory and one issued packet")
  {
    constexpr std::size_t levels = 5;
    MEMORY_CONTROLLER dram{champsim::modules::ModuleBuilder{"t600_dram_2", "DEFAULT_MEMORY_CONTROLLER", champsim::defaults::default_memory_controller()}};
    VirtualMemory vmem{champsim::modules::ModuleBuilder{"t600_vmem_2", "DEFAULT_VMEM", champsim::defaults::default_vmem()}
        .add_parameter("dram", static_cast<champsim::modules::memory_controller_module*>(&dram))
        .add_parameter("page_table_levels", levels)
        .add_parameter("minor_fault_penalty", champsim::chrono::picoseconds{640000})};
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    PageTableWalker uut{champsim::modules::ModuleBuilder{"t600_ptw_2", "DEFAULT_PTW", champsim::defaults::default_ptw()}
                            .add_parameter("clock_period", champsim::chrono::picoseconds{3200})
                            .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                            .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                            .add_parameter("vmem", static_cast<champsim::modules::vmem_module*>(&vmem))
                            .add_parameter("pscl_dims", std::vector<std::array<uint32_t, 3>>{{5, 1, 1}, {4, 1, 1}, {3, 1, 1}, {2, 1, 1}})};

    std::array<champsim::operable*, 3> elements{{&mock_ul, &uut, &mock_ll}};

    uut.begin_phase(false);

    decltype(mock_ul)::request_type seed;
    seed.address = champsim::address{0xffff'ffff'ffff'ffff};
    seed.v_address = seed.address;
    seed.origin = champsim::origin{0, 0};

    auto seed_result = mock_ul.issue(seed);
    REQUIRE(seed_result);

    for (auto i = 0; i < 10000; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("The PTW receives the same request")
    {
      mock_ll.addresses.clear();

      auto test_result = mock_ul.issue(seed);
      REQUIRE(test_result);

      for (auto i = 0; i < 10000; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("2 requests are issued")
      {
        REQUIRE(mock_ll.packet_count() == 2);
        REQUIRE(mock_ul.packets.back().return_time > 0);
      }
    }

    WHEN("The PTW receives a nearby request")
    {
      mock_ll.addresses.clear();

      decltype(mock_ul)::request_type test = seed;
      test.address = champsim::address{0xffff'ffff'ffc0'0000};
      test.v_address = test.address;
      auto test_result = mock_ul.issue(test);
      REQUIRE(test_result);

      for (auto i = 0; i < 10000; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("3 requests are issued")
      {
        REQUIRE(mock_ll.packet_count() == 3);
        REQUIRE(mock_ul.packets.back().return_time > 0);
      }
    }

    WHEN("The PTW receives a less-nearby request")
    {
      mock_ll.addresses.clear();

      decltype(mock_ul)::request_type test = seed;
      test.address = champsim::address{0xffff'ffff'8000'0000};
      test.v_address = test.address;
      auto test_result = mock_ul.issue(test);
      REQUIRE(test_result);

      for (auto i = 0; i < 10000; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("4 requests are issued")
      {
        REQUIRE(mock_ll.packet_count() == 4);
        REQUIRE(mock_ul.packets.back().return_time > 0);
      }
    }

    WHEN("The PTW receives a distant request")
    {
      mock_ll.addresses.clear();

      decltype(mock_ul)::request_type test = seed;
      test.address = champsim::address{0xffff'ff00'0000'0000};
      test.v_address = test.address;
      auto test_result = mock_ul.issue(test);
      REQUIRE(test_result);

      for (auto i = 0; i < 10000; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("5 requests are issued")
      {
        REQUIRE(mock_ll.packet_count() == 5);
        REQUIRE(mock_ul.packets.back().return_time > 0);
      }
    }

    WHEN("The PTW receives a very distant request")
    {
      mock_ll.addresses.clear();

      decltype(mock_ul)::request_type test = seed;
      test.address = champsim::address{0xfffe'0000'0000'0000};
      test.v_address = test.address;
      auto test_result = mock_ul.issue(test);
      REQUIRE(test_result);

      for (auto i = 0; i < 10000; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("5 requests are issued")
      {
        REQUIRE(mock_ll.packet_count() == 5);
        REQUIRE(mock_ul.packets.back().return_time > 0);
      }
    }
  }
}
