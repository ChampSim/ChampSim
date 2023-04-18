#include <catch.hpp>
#include "mocks.hpp"
#include "ooo_cpu.h"
#include "instr.h"

SCENARIO("An empty ROB retires no instructions") {
  GIVEN("An empty ROB") {
    do_nothing_MRC mock_L1I, mock_L1D;
    constexpr std::size_t retire_bandwidth = 1;
    O3_CPU uut{0, 1.0, {32, 8, {2}, {2}}, 64, 32, 32, 352, 128, 72, 2, 2, 2, 128, 1, 2, 2, retire_bandwidth, 1, 1, 1, 0, 0, nullptr, &mock_L1I.queues, 1, &mock_L1D.queues, 1, O3_CPU::bbranchDbimodal, O3_CPU::tbtbDbasic_btb};

    auto old_rob_occupancy = std::size(uut.ROB);
    auto old_num_retired = uut.num_retired;

    WHEN("A cycle happens") {
      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();

      THEN("The number of retired instructions stays the same") {
        REQUIRE(std::size(uut.ROB) == old_rob_occupancy);
        REQUIRE(uut.num_retired == old_num_retired);
      }
    }
  }
}

SCENARIO("A ROB with uncompleted instructions retires no instructions") {
  GIVEN("A ROB with a single instruction") {
    do_nothing_MRC mock_L1I, mock_L1D;
    constexpr std::size_t retire_bandwidth = 1;
    O3_CPU uut{0, 1.0, {32, 8, {2}, {2}}, 64, 32, 32, 352, 128, 72, 2, 2, 2, 128, 1, 2, 2, retire_bandwidth, 1, 1, 1, 0, 0, nullptr, &mock_L1I.queues, 1, &mock_L1D.queues, 1, O3_CPU::bbranchDbimodal, O3_CPU::tbtbDbasic_btb};

    uut.ROB.push_back(champsim::test::instruction_with_ip(1));

    auto old_rob_occupancy = std::size(uut.ROB);
    auto old_num_retired = uut.num_retired;

    WHEN("The instruction is not executed") {
      uut.ROB.front().executed = 0;
      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();

      THEN("The number of retired instructions stays the same") {
        REQUIRE(std::size(uut.ROB) == old_rob_occupancy);
        REQUIRE(uut.num_retired == old_num_retired);
      }
    }

    WHEN("The instruction has been executed") {
      uut.ROB.front().executed = COMPLETED;
      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();

      THEN("The instruction is retired") {
        REQUIRE(std::size(uut.ROB) == 0);
        REQUIRE(uut.num_retired == old_num_retired+1);
      }
    }
  }
}

SCENARIO("A ROB retires only the head instruction") {
  GIVEN("A ROB with two instructions") {
    do_nothing_MRC mock_L1I, mock_L1D;
    constexpr std::size_t retire_bandwidth = 2;
    O3_CPU uut{0, 1.0, {32, 8, {2}, {2}}, 64, 32, 32, 352, 128, 72, 2, 2, 2, 128, 1, 2, 2, retire_bandwidth, 1, 1, 1, 0, 0, nullptr, &mock_L1I.queues, 1, &mock_L1D.queues, 1, O3_CPU::bbranchDbimodal, O3_CPU::tbtbDbasic_btb};

    std::vector test_instructions( retire_bandwidth, champsim::test::instruction_with_ip(1) );

    uut.ROB.insert(std::end(uut.ROB), std::begin(test_instructions), std::end(test_instructions));

    auto old_rob_occupancy = std::size(uut.ROB);
    auto old_num_retired = uut.num_retired;

    WHEN("The second instruction is executed") {
      uut.ROB[0].executed = 0;
      uut.ROB[1].executed = COMPLETED;

      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();

      THEN("No instructions are retired") {
        REQUIRE(std::size(uut.ROB) == old_rob_occupancy);
        REQUIRE(uut.num_retired == old_num_retired);
      }
    }

    WHEN("Both instructions are executed") {
      uut.ROB[0].executed = COMPLETED;
      uut.ROB[1].executed = COMPLETED;

      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();

      THEN("Both instructions are retired") {
        REQUIRE(std::size(uut.ROB) == 0);
        REQUIRE(uut.num_retired == old_num_retired+std::size(test_instructions));
      }
    }
  }
}

SCENARIO("A ROB's retirement is bandwidth-limited") {
  GIVEN("A ROB with twice as many instructions as retire bandwidth") {
    do_nothing_MRC mock_L1I, mock_L1D;
    constexpr std::size_t retire_bandwidth = 1;
    O3_CPU uut{0, 1.0, {32, 8, {2}, {2}}, 64, 32, 32, 352, 128, 72, 2, 2, 2, 128, 1, 2, 2, retire_bandwidth, 1, 1, 1, 0, 0, nullptr, &mock_L1I.queues, 1, &mock_L1D.queues, 1, O3_CPU::bbranchDbimodal, O3_CPU::tbtbDbasic_btb};

    std::vector test_instructions( 2*retire_bandwidth, champsim::test::instruction_with_ip(1) );

    uut.ROB.insert(std::end(uut.ROB), std::begin(test_instructions), std::end(test_instructions));

    auto old_rob_occupancy = std::size(uut.ROB);
    auto old_num_retired = uut.num_retired;

    WHEN("All instructions are executed") {
      uut.ROB[0].executed = COMPLETED;
      uut.ROB[1].executed = COMPLETED;

      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();

      THEN("The bandwidth of instructions are retired") {
        REQUIRE_THAT(uut.ROB, Catch::Matchers::SizeIs(old_rob_occupancy-static_cast<std::size_t>(uut.RETIRE_WIDTH)));
        REQUIRE(uut.num_retired == old_num_retired+static_cast<std::size_t>(uut.RETIRE_WIDTH));
      }

      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();

      AND_THEN("The remaining instructions are retired") {
        REQUIRE(std::size(uut.ROB) == 0);
        REQUIRE(uut.num_retired == old_num_retired+std::size(test_instructions));
      }
    }
  }
}
