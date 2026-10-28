#include <array>
#include <catch.hpp>

#include "defaults.hpp"
#include "dram_controller.h"
#include "mocks.hpp"
#include "ptw.h"
#include "vmem.h"

SCENARIO("One page table walker serves multiple address spaces")
{
  GIVEN("A PTW with PSCLs and two streams issuing the same virtual address")
  {
    constexpr std::size_t levels = 5;
    MEMORY_CONTROLLER dram{champsim::modules::ModuleBuilder{"t604_dram_0", "DEFAULT_MEMORY_CONTROLLER", champsim::defaults::default_memory_controller()}};
    VirtualMemory vmem{champsim::modules::ModuleBuilder{"t604_vmem_0", "DEFAULT_VMEM", champsim::defaults::default_vmem()}
                           .add_parameter("dram", static_cast<champsim::modules::memory_controller_module*>(&dram))
                           .add_parameter("page_table_levels", levels)
                           .add_parameter("minor_fault_penalty", champsim::chrono::picoseconds{640000})};
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    PageTableWalker uut{champsim::modules::ModuleBuilder{"t604_ptw_0", "DEFAULT_PTW", champsim::defaults::default_ptw()}
                            .add_parameter("clock_period", champsim::chrono::picoseconds{3200})
                            .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                            .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                            .add_parameter("vmem", static_cast<champsim::modules::vmem_module*>(&vmem))
                            .add_parameter("pscl_dims", std::vector<std::array<uint32_t, 3>>{{5, 1, 1}, {4, 1, 1}, {3, 1, 1}, {2, 1, 1}})};

    std::array<champsim::operable*, 3> elements{{&mock_ul, &uut, &mock_ll}};

    uut.begin_phase(false);

    auto drive = [&](champsim::origin origin) {
      decltype(mock_ul)::request_type test;
      test.address = champsim::address{0xdeadbeef};
      test.v_address = test.address;
      test.origin = origin;

      auto before = mock_ul.packets.size();
      REQUIRE(mock_ul.issue(test));
      for (auto i = 0; i < 10000; ++i) {
        for (auto* elem : elements) {
          elem->_operate();
        }
      }
      return mock_ul.packets.at(before);
    };

    WHEN("Stream 0 walks, then stream 1 walks the same virtual address")
    {
      auto first_steps_before = mock_ll.packet_count();
      auto result0 = drive(champsim::origin{0, 0});
      auto first_steps = mock_ll.packet_count() - first_steps_before;

      auto second_steps_before = mock_ll.packet_count();
      auto result1 = drive(champsim::origin{0, 1});
      auto second_steps = mock_ll.packet_count() - second_steps_before;

      THEN("Both walks complete")
      {
        REQUIRE(result0.return_time > 0);
        REQUIRE(result1.return_time > 0);
      }

      THEN("The second stream cannot hit the first stream's cached walk steps")
      {
        // A same-stream repeat would hit the PSCLs and skip levels; a
        // different stream must issue the full walk.
        REQUIRE(first_steps == levels);
        REQUIRE(second_steps == levels);
      }
    }

    WHEN("The same stream walks the same virtual address twice")
    {
      drive(champsim::origin{0, 0});
      auto repeat_steps_before = mock_ll.packet_count();
      drive(champsim::origin{0, 0});
      auto repeat_steps = mock_ll.packet_count() - repeat_steps_before;

      THEN("The repeat walk hits the PSCLs and skips levels")
      {
        REQUIRE(repeat_steps < levels);
      }
    }
  }
}
