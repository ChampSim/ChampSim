#include <catch.hpp>

#include <algorithm>
#include <array>
#include <cstdint>

#include "instr.h"
#include "mocks.hpp"
#include "ooo_cpu.h"
#include "defaults.hpp"

// The free-physical-register rename gate must apply only to instructions being
// renamed this cycle. An already-scheduled instruction has already claimed its
// registers, so evaluating the gate against it -- and breaking scheduling when
// the free pool is smaller than its destination count -- wrongly stalls ready
// instructions sitting behind it in the ROB whenever registers are scarce.
SCENARIO("An exhausted register file does not block scheduling behind an already-scheduled instruction")
{
  GIVEN("A core whose register file is drained by scheduling a ROB full of in-flight writers")
  {
    constexpr unsigned schedule_width = 128;
    constexpr unsigned schedule_latency = 1;
    constexpr uint32_t register_file_size = 32;

    do_nothing_MRC mock_L1I, mock_L1D;
    O3_CPU uut{champsim::modules::ModuleBuilder{"t202_core", "DEFAULT_CORE", test_core_defaults("t202_core_ws")}
                   .add_parameter("schedule_width", champsim::bandwidth::maximum_type{schedule_width})
                   .add_parameter("register_file_size", register_file_size)
                   .add_parameter("schedule_latency", static_cast<unsigned>(schedule_latency))
                   .add_parameter("fetch_queues", static_cast<champsim::modules::channel_module*>(&mock_L1I.queues))
                   .add_parameter("data_queues", static_cast<champsim::modules::channel_module*>(&mock_L1D.queues))};

    std::array<champsim::operable*, 3> elements{{&uut, &mock_L1I, &mock_L1D}};

    // Fill the ROB with as many single-destination writers as there are free
    // physical registers. Each one claims exactly one register when the scheduler
    // renames it, so scheduling the whole batch drains the file through the real
    // rename path -- no poking the allocator directly.
    const auto writer_count = uut.reg_allocator.count_free_registers();
    REQUIRE(writer_count > 0);
    for (uint64_t i = 0; i < writer_count; ++i) {
      auto writer = champsim::test::instruction_with_ip(1 + i);
      writer.instr_id = 1 + i;
      writer.destination_registers.push_back(10);
      writer.ready_time = champsim::chrono::clock::time_point{};
      uut.ROB.push_back(writer);
    }

    WHEN("The core cycles so every writer is scheduled and the register file is exhausted")
    {
      for (auto op : elements)
        op->_operate();

      REQUIRE(std::all_of(std::begin(uut.ROB), std::end(uut.ROB), [](const auto& instr) { return instr.scheduled; }));
      REQUIRE(uut.reg_allocator.count_free_registers() == 0);

      AND_WHEN("A ready instruction needing no new registers arrives behind the in-flight writers and the core cycles again")
      {
        auto probe = champsim::test::instruction_with_ip(1000);
        probe.instr_id = 1000;
        probe.ready_time = champsim::chrono::clock::time_point{};
        uut.ROB.push_back(probe);

        for (auto op : elements)
          op->_operate();

        THEN("It still schedules -- the exhausted file must not gate an already-scheduled instruction ahead of it")
        {
          REQUIRE(uut.ROB.back().scheduled);
        }
      }
    }
  }
}
