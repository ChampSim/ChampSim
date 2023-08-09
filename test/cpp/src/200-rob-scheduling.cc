#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "ooo_cpu.h"
#include "instr.h"

SCENARIO("The scheduler can detect RAW hazards") {
  GIVEN("A ROB with a single instruction") {
    constexpr unsigned schedule_width = 128;
    constexpr unsigned schedule_latency = 1;

    do_nothing_MRC mock_L1I, mock_L1D;
    O3_CPU uut{O3_CPU::Builder{champsim::defaults::default_core}
      .schedule_width(schedule_width)
      .schedule_latency(schedule_latency)
      .fetch_queues(&mock_L1I.queues)
      .data_queues(&mock_L1D.queues)
    };

    uut.ROB.push_back(champsim::test::instruction_with_ip(1));
    for (auto &instr : uut.ROB)
      instr.event_cycle = uut.current_cycle;

    //auto old_cycle = uut.current_cycle;

    WHEN("The instruction is not scheduled") {
      uut.ROB.front().scheduled = 0;
      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();

      THEN("The instruction has no register dependencies") {
        REQUIRE(uut.ROB.front().num_reg_dependent == 0);
        REQUIRE(uut.ROB.front().scheduled == COMPLETED);
        //REQUIRE(uut.ROB.front().event_cycle == old_cycle + schedule_latency);
      }
    }
  }

  GIVEN("A ROB with a RAW hazard") {
    constexpr unsigned schedule_width = 128;
    constexpr unsigned schedule_latency = 1;

    do_nothing_MRC mock_L1I, mock_L1D;
    O3_CPU uut{O3_CPU::Builder{champsim::defaults::default_core}
      .schedule_width(schedule_width)
      .schedule_latency(schedule_latency)
      .fetch_queues(&mock_L1I.queues)
      .data_queues(&mock_L1D.queues)
    };

    std::vector test_instructions( 2, champsim::test::instruction_with_registers(42) );

    std::copy(std::begin(test_instructions), std::end(test_instructions), std::back_inserter(uut.ROB));
    for (auto &instr : uut.ROB)
      instr.event_cycle = uut.current_cycle;

    //auto old_cycle = uut.current_cycle;

    WHEN("None of the instructions are scheduled") {
      for (auto &instr : uut.ROB)
        instr.scheduled = 0;

      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();

      THEN("The second instruction is dependent on the first") {
        REQUIRE(uut.ROB[0].num_reg_dependent == 0);
        REQUIRE(uut.ROB[1].num_reg_dependent == 1);
        REQUIRE(uut.ROB[0].scheduled == COMPLETED);
        REQUIRE(uut.ROB[1].scheduled == COMPLETED);
        //REQUIRE(uut.ROB[0].event_cycle == old_cycle + schedule_latency);
        //REQUIRE(uut.ROB[1].event_cycle == old_cycle + schedule_latency);
      }
    }
  }

  GIVEN("A ROB with more hazards than the scheduler can handle") {
    constexpr unsigned schedule_width = 4;
    constexpr unsigned schedule_latency = 1;

    do_nothing_MRC mock_L1I, mock_L1D;
    O3_CPU uut{O3_CPU::Builder{champsim::defaults::default_core}
      .schedule_width(schedule_width)
      .schedule_latency(schedule_latency)
      .fetch_queues(&mock_L1I.queues)
      .data_queues(&mock_L1D.queues)
    };

    std::vector test_instructions( schedule_width + 1, champsim::test::instruction_with_registers(42) );

    std::copy(std::begin(test_instructions), std::end(test_instructions), std::back_inserter(uut.ROB));
    uint64_t id = 0;
    for (auto &instr : uut.ROB) {
      instr.instr_id = id++;
      instr.event_cycle = uut.current_cycle;
    }

    //auto old_cycle = uut.current_cycle;

    WHEN("None of the instructions are scheduled") {
      for (auto &instr : uut.ROB) {
        instr.scheduled = 0;
        instr.executed = 0;
      }

      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();

      THEN("The second instruction is dependent on the first") {
        REQUIRE(uut.ROB.at(0).num_reg_dependent >= 0);
        REQUIRE(uut.ROB.at(1).num_reg_dependent >= 1);
        REQUIRE(uut.ROB.at(2).num_reg_dependent >= 1);
        REQUIRE(uut.ROB.at(3).num_reg_dependent >= 1);
        REQUIRE(uut.ROB.at(4).num_reg_dependent >= 0);
        //REQUIRE(std::all_of(std::next(std::begin(uut.ROB)), std::next(std::begin(uut.ROB), schedule_width), [](ooo_model_instr x){ return x.num_reg_dependent >= 1; }));
        //REQUIRE(uut.ROB.back().num_reg_dependent == 0);

        REQUIRE(uut.ROB.at(0).scheduled == COMPLETED);
        REQUIRE(uut.ROB.at(1).scheduled == COMPLETED);
        REQUIRE(uut.ROB.at(2).scheduled == COMPLETED);
        REQUIRE(uut.ROB.at(3).scheduled == COMPLETED);
        REQUIRE(uut.ROB.at(4).scheduled == 0);
        //REQUIRE(std::all_of(std::next(std::begin(uut.ROB)), std::next(std::begin(uut.ROB), schedule_width), [](ooo_model_instr x){ return x.scheduled == COMPLETED; }));
        //REQUIRE(uut.ROB.back().scheduled == 0);

        //REQUIRE(uut.ROB[0].event_cycle == old_cycle + schedule_latency);
        //REQUIRE(std::all_of(std::next(std::begin(uut.ROB)), std::next(std::begin(uut.ROB), schedule_width), [old_cycle](ooo_model_instr x){ return x.event_cycle == old_cycle + schedule_latency; }));
        //REQUIRE(uut.ROB.back().event_cycle == old_cycle);
      }
    }
  }
}

