#include <catch.hpp>
#include "mocks.hpp"
#include "ooo_cpu.h"
#include "instr.h"

SCENARIO("The scheduler can detect RAW hazards") {
  GIVEN("A ROB with a single instruction") {
    constexpr unsigned schedule_width = 128;
    constexpr unsigned schedule_latency = 1;

    do_nothing_MRC mock_L1I, mock_L1D;
    O3_CPU uut{champsim::core_builder{}
      .schedule_width(champsim::bandwidth::maximum_type{schedule_width})
      .register_file_size(128)
      .schedule_latency(schedule_latency)
      .fetch_queues(&mock_L1I.queues)
      .data_queues(&mock_L1D.queues)
    };

    uut.ROB.push_back(champsim::test::instruction_with_ip(1));
    for (auto &instr : uut.ROB)
      instr.ready_time = champsim::chrono::clock::time_point{};

    //auto old_cycle = uut.current_cycle();

    WHEN("The instruction is not scheduled") {
      uut.ROB.front().scheduled = 0;
      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();

      THEN("The instruction has no register dependencies") {
        REQUIRE(uut.reg_allocator.count_reg_dependencies(uut.ROB.front()) == 0);
        REQUIRE(uut.ROB.front().scheduled);
        //REQUIRE(uut.ROB.front().event_cycle == old_cycle + schedule_latency);
      }
    }
  }

  GIVEN("A ROB with a RAW hazard") {
    constexpr unsigned schedule_width = 128;
    constexpr unsigned schedule_latency = 1;

    do_nothing_MRC mock_L1I, mock_L1D;
    O3_CPU uut{champsim::core_builder{}
      .schedule_width(champsim::bandwidth::maximum_type{schedule_width})
      .register_file_size(128)
      .schedule_latency(schedule_latency)
      .fetch_queues(&mock_L1I.queues)
      .data_queues(&mock_L1D.queues)
    };

    std::vector test_instructions( 2, champsim::test::instruction_with_registers(42) );

    std::copy(std::begin(test_instructions), std::end(test_instructions), std::back_inserter(uut.ROB));
    for (auto &instr : uut.ROB)
      instr.ready_time = champsim::chrono::clock::time_point{};

    //auto old_cycle = uut.current_cycle();

    WHEN("None of the instructions are scheduled") {
      for (auto &instr : uut.ROB)
        instr.scheduled = 0;

      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();

      THEN("The second instruction is dependent on the first") {
        REQUIRE(uut.reg_allocator.count_reg_dependencies(uut.ROB.at(0)) == 0);
        REQUIRE(uut.reg_allocator.count_reg_dependencies(uut.ROB.at(1)) == 1);
        REQUIRE(uut.ROB.at(0).scheduled);
        REQUIRE(uut.ROB.at(1).scheduled);
        //REQUIRE(uut.ROB[0].event_cycle == old_cycle + schedule_latency);
        //REQUIRE(uut.ROB[1].event_cycle == old_cycle + schedule_latency);
      }
    }
  }

  GIVEN("A ROB with more hazards than the scheduler can handle") {
    constexpr unsigned schedule_width = 4;
    constexpr unsigned schedule_latency = 1;

    do_nothing_MRC mock_L1I, mock_L1D;
    O3_CPU uut{champsim::core_builder{}
      .schedule_width(champsim::bandwidth::maximum_type{schedule_width})
      .register_file_size(128)
      .schedule_latency(schedule_latency)
      .fetch_queues(&mock_L1I.queues)
      .data_queues(&mock_L1D.queues)
    };

    std::vector test_instructions( schedule_width + 1, champsim::test::instruction_with_registers(42) );

    std::copy(std::begin(test_instructions), std::end(test_instructions), std::back_inserter(uut.ROB));
    uint64_t id = 0;
    for (auto &instr : uut.ROB) {
      instr.instr_id = id++;
      instr.ready_time = champsim::chrono::clock::time_point{};
    }

    //auto old_cycle = uut.current_cycle();

    WHEN("None of the instructions are scheduled") {
      for (auto &instr : uut.ROB) {
        instr.scheduled = 0;
        instr.executed = 0;
      }

      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();

      THEN("The second instruction is dependent on the first") {
        REQUIRE(uut.reg_allocator.count_reg_dependencies(uut.ROB[0]) >= 0);
        REQUIRE(uut.reg_allocator.count_reg_dependencies(uut.ROB[1]) >= 1);
        REQUIRE(uut.reg_allocator.count_reg_dependencies(uut.ROB[2]) >= 1);
        REQUIRE(uut.reg_allocator.count_reg_dependencies(uut.ROB[3]) >= 1);
        REQUIRE(uut.reg_allocator.count_reg_dependencies(uut.ROB[4]) >= 0);
        //REQUIRE(std::all_of(std::next(std::begin(uut.ROB)), std::next(std::begin(uut.ROB), schedule_width), [](ooo_model_instr x){ return x.num_reg_dependent >= 1; }));
        //REQUIRE(uut.ROB.back().num_reg_dependent == 0);

        REQUIRE(uut.ROB.at(0).scheduled);
        REQUIRE(uut.ROB.at(1).scheduled);
        REQUIRE(uut.ROB.at(2).scheduled);
        REQUIRE(uut.ROB.at(3).scheduled);
        REQUIRE_FALSE(uut.ROB.at(4).scheduled);
        //REQUIRE(std::all_of(std::next(std::begin(uut.ROB)), std::next(std::begin(uut.ROB), schedule_width), [](ooo_model_instr x){ return x.scheduled; }));
        //REQUIRE_FALSE(uut.ROB.back().scheduled);

        //REQUIRE(uut.ROB[0].event_cycle == old_cycle + schedule_latency);
        //REQUIRE(std::all_of(std::next(std::begin(uut.ROB)), std::next(std::begin(uut.ROB), schedule_width), [old_cycle](ooo_model_instr x){ return x.event_cycle == old_cycle + schedule_latency; }));
        //REQUIRE(uut.ROB.back().event_cycle == old_cycle);
      }
    }
  }
}

SCENARIO("The scheduler handles WAW hazards") {
  GIVEN("A ROB with a WAW hazard followed by a read from that register") {
    constexpr unsigned schedule_width = 128;
    constexpr unsigned schedule_latency = 1;
    constexpr unsigned execute_width = 3;
    constexpr unsigned execute_latency = 3;

    do_nothing_MRC mock_L1I, mock_L1D;
    O3_CPU uut{champsim::core_builder{}
      .schedule_width(champsim::bandwidth::maximum_type{schedule_width})
      .register_file_size(128)
      .schedule_latency(schedule_latency)
      .execute_latency(execute_latency)
      .execute_width(champsim::bandwidth::maximum_type{execute_width})
      .retire_width(champsim::bandwidth::maximum_type{execute_width})
      .fetch_queues(&mock_L1I.queues)
      .data_queues(&mock_L1D.queues)
    };

    uut.ROB.push_back(champsim::test::instruction_with_ip(1));
    uut.ROB.at(0).instr_id = 1;
    uut.ROB.at(0).destination_registers.push_back(5);
    uut.ROB.push_back(champsim::test::instruction_with_ip(2));
    uut.ROB.at(1).instr_id = 2;
    uut.ROB.at(1).destination_registers.push_back(5);
    uut.ROB.push_back(champsim::test::instruction_with_ip(3));
    uut.ROB.at(2).instr_id = 3;
    uut.ROB.at(2).source_registers.push_back(5);
    for (auto &instr : uut.ROB)
      instr.ready_time = champsim::chrono::clock::time_point{};

    WHEN("The first two instructions are in flight") {
      uut.ROB.at(0).scheduled = false;
      uut.ROB.at(1).scheduled = false;
      uut.ROB.at(2).scheduled = false;
      // Schedule
      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();
      REQUIRE(uut.reg_allocator.count_reg_dependencies(uut.ROB.at(2)) == 1);
      // Execute
      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();

      THEN("The third instruction does not execute") {
        REQUIRE(uut.ROB.at(0).executed == true);
        REQUIRE(uut.ROB.at(1).executed == true);
        REQUIRE(uut.ROB.at(2).executed == false);
        REQUIRE(uut.reg_allocator.count_reg_dependencies(uut.ROB[2]) == 1);
      }
      AND_WHEN("The first instruction finishes executing first"){
        REQUIRE(uut.ROB.at(1).executed == true);
        REQUIRE(uut.ROB.at(1).completed == false);
        uut.ROB.at(1).ready_time = champsim::chrono::clock::time_point{} + 5 * uut.EXEC_LATENCY;
        for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
          op->_operate();
        THEN("The third instruction does not execute"){
          REQUIRE(uut.ROB.at(0).completed == true);
          REQUIRE(uut.ROB.at(1).completed == false);
          REQUIRE(uut.ROB.at(2).executed == false);
        }
      }
      AND_WHEN("The second instruction finishes executing while the first is still in flight"){
        REQUIRE(uut.ROB.at(0).executed == true);
        REQUIRE(uut.ROB.at(0).completed == false);
        uut.ROB.at(0).ready_time = champsim::chrono::clock::time_point{} + 5 * uut.EXEC_LATENCY;
        for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
          op->_operate();
        THEN("The third instruction executes"){
          REQUIRE(uut.ROB.at(2).executed == true);
        }
      }
    }
  }

  GIVEN("A ROB with a long-running instruction that writes to a register") {
    constexpr unsigned schedule_width = 128;
    constexpr unsigned schedule_latency = 1;
    constexpr unsigned execute_width = 3;
    constexpr unsigned execute_latency = 3;

    do_nothing_MRC mock_L1I, mock_L1D;
    O3_CPU uut{champsim::core_builder{}
      .schedule_width(champsim::bandwidth::maximum_type{schedule_width})
      .schedule_latency(schedule_latency)
      .execute_latency(execute_latency)
      .register_file_size(128)
      .execute_width(champsim::bandwidth::maximum_type{execute_width})
      .retire_width(champsim::bandwidth::maximum_type{execute_width})
      .fetch_queues(&mock_L1I.queues)
      .data_queues(&mock_L1D.queues)
    };

    uut.ROB.push_back(champsim::test::instruction_with_ip(1));
    uut.ROB.at(0).instr_id = 1;
    uut.ROB.at(0).destination_registers.push_back(5);
    uut.ROB.at(0).source_memory.push_back(champsim::address{0xDEADBEEF});
    uut.ROB.at(0).ready_time = champsim::chrono::clock::time_point{};
    uut.ROB.at(0).scheduled = false;
    for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();
    for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();

    WHEN("An write to the same register and a read from the register are scheduled") {
      uut.ROB.push_back(champsim::test::instruction_with_ip(2));
      uut.ROB.at(1).instr_id = 2;
      uut.ROB.at(1).destination_registers.push_back(5);
      uut.ROB.push_back(champsim::test::instruction_with_ip(3));
      uut.ROB.at(2).instr_id = 3;
      uut.ROB.at(2).source_registers.push_back(5);
      for (auto &instr : uut.ROB)
        instr.ready_time = champsim::chrono::clock::time_point{};
      uut.ROB.at(1).scheduled = false;
      uut.ROB.at(2).scheduled = false;
      // Schedule 1,2
      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();
      REQUIRE(uut.reg_allocator.count_reg_dependencies(uut.ROB[2]) == 1);
      // Execute 1,2
      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();

      THEN("The third instruction does not execute and depends on the second write") {
        REQUIRE(uut.ROB.at(0).completed == false);
        REQUIRE(uut.ROB.at(1).completed == false);
        REQUIRE(uut.ROB.at(2).executed == false);
      }

      AND_WHEN("The second instruction finishes executing while the first is still in flight"){
        REQUIRE(uut.ROB.at(0).executed == true);
        REQUIRE(uut.ROB.at(0).completed == false);
        for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
          op->_operate();
        THEN("The third instruction executes"){
          REQUIRE(uut.ROB.at(1).completed == true);
          REQUIRE(uut.ROB.at(2).executed == true);
        }
      }
    }
  }
}

TEST_CASE("ooo_cpu Benchmarks") {
  BENCHMARK_ADVANCED("ooo_cpu::operate()")(Catch::Benchmark::Chronometer meter){
    constexpr unsigned schedule_width = 128;
    constexpr unsigned schedule_latency = 1;
    constexpr unsigned execute_width = 3;
    constexpr unsigned execute_latency = 3;

    do_nothing_MRC mock_L1I, mock_L1D;
    O3_CPU uut{champsim::core_builder{}
      .schedule_width(champsim::bandwidth::maximum_type{schedule_width})
      .schedule_latency(schedule_latency)
      .execute_latency(execute_latency)
      .register_file_size(128)
      .execute_width(champsim::bandwidth::maximum_type{execute_width})
      .retire_width(champsim::bandwidth::maximum_type{execute_width})
      .fetch_queues(&mock_L1I.queues)
      .data_queues(&mock_L1D.queues)
    };

      uut.ROB.push_back(champsim::test::instruction_with_ip(1));
      uut.ROB.at(0).instr_id = 1;
      uut.ROB.at(0).destination_registers.push_back(5);
      uut.ROB.at(0).source_memory.push_back(champsim::address{0xDEADBEEF});
      uut.ROB.at(0).ready_time = champsim::chrono::clock::time_point{};
      uut.ROB.at(0).scheduled = false;

      meter.measure([&] { for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
                          op->_operate();});
      meter.measure([&] {for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
                          op->_operate();});

        uut.ROB.push_back(champsim::test::instruction_with_ip(2));
        uut.ROB.at(1).instr_id = 2;
        uut.ROB.at(1).destination_registers.push_back(5);
        uut.ROB.push_back(champsim::test::instruction_with_ip(3));
        uut.ROB.at(2).instr_id = 3;
        uut.ROB.at(2).source_registers.push_back(5);
        for (auto &instr : uut.ROB)
          instr.ready_time = champsim::chrono::clock::time_point{};
        uut.ROB.at(1).scheduled = false;
        uut.ROB.at(2).scheduled = false;
        // Schedule 1,2

        meter.measure([&] { for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
          op->_operate();});
        // Execute 1,2
        meter.measure([&] { for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
          op->_operate();});
        meter.measure([&] { for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
          op->_operate();});
  };
  SUCCEED();
}