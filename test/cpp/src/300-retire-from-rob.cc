#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "ooo_cpu.h"
#include "instr.h"

SCENARIO("Completed instructions are retired") {
  GIVEN("An empty ROB") {
    do_nothing_MRC mock_L1D;
    constexpr std::size_t retire_bandwidth = 1;
    champsim::reorder_buffer uut{0, 352, 128, 72, 128, 1, 2, 2, 1, retire_bandwidth, 1, 1, 0, &mock_L1D.queues};

    auto old_rob_occupancy = std::size(uut.ROB);
    auto old_num_retired = uut.retired_count();

    WHEN("A cycle happens") {
      uut.operate();
      mock_L1D.operate();

      THEN("The number of retired instructions stays the same") {
        REQUIRE(std::size(uut.ROB) == old_rob_occupancy);
        REQUIRE(uut.retired_count() == old_num_retired);
      }
    }
  }

  GIVEN("A ROB with a single instruction") {
    do_nothing_MRC mock_L1D;
    constexpr std::size_t retire_bandwidth = 1;
    champsim::reorder_buffer uut{0, 352, 128, 72, 128, 1, 2, 2, 1, retire_bandwidth, 1, 1, 0, &mock_L1D.queues};

    uut.ROB.push_back(champsim::test::instruction_with_ip(1));

    auto old_rob_occupancy = std::size(uut.ROB);
    auto old_num_retired = uut.retired_count();

    WHEN("The instruction is not executed") {
      uut.ROB.front().executed = 0;
      uut.operate();
      mock_L1D.operate();

      THEN("The number of retired instructions stays the same") {
        REQUIRE(std::size(uut.ROB) == old_rob_occupancy);
        REQUIRE(uut.retired_count() == old_num_retired);
      }
    }

    WHEN("The instruction has been executed") {
      uut.ROB.front().executed = COMPLETED;
      uut.operate();
      mock_L1D.operate();

      THEN("The instruction is retired") {
        REQUIRE(std::size(uut.ROB) == 0);
        REQUIRE(uut.retired_count() == old_num_retired+1);
      }
    }
  }

  GIVEN("A ROB with two instructions") {
    do_nothing_MRC mock_L1D;
    constexpr std::size_t retire_bandwidth = 2;
    champsim::reorder_buffer uut{0, 352, 128, 72, 128, 1, 2, 2, 1, retire_bandwidth, 1, 1, 0, &mock_L1D.queues};

    std::vector test_instructions( retire_bandwidth, champsim::test::instruction_with_ip(1) );

    uut.ROB.insert(std::end(uut.ROB), std::begin(test_instructions), std::end(test_instructions));

    auto old_rob_occupancy = std::size(uut.ROB);
    auto old_num_retired = uut.retired_count();

    WHEN("The second instruction is executed") {
      uut.ROB[0].executed = 0;
      uut.ROB[1].executed = COMPLETED;

      uut.operate();
      mock_L1D.operate();

      THEN("No instructions are retired") {
        REQUIRE(std::size(uut.ROB) == old_rob_occupancy);
        REQUIRE(uut.retired_count() == old_num_retired);
      }
    }

    WHEN("Both instructions are executed") {
      uut.ROB[0].executed = COMPLETED;
      uut.ROB[1].executed = COMPLETED;

      uut.operate();
      mock_L1D.operate();

      THEN("Both instructions are retired") {
        REQUIRE(std::size(uut.ROB) == 0);
        REQUIRE(uut.retired_count() == old_num_retired+std::size(test_instructions));
      }
    }
  }

  GIVEN("A ROB with twice as many instructions as retire bandwidth") {
    do_nothing_MRC mock_L1D;
    constexpr std::size_t retire_bandwidth = 1;
    champsim::reorder_buffer uut{0, 352, 128, 72, 128, 1, 2, 2, 1, retire_bandwidth, 1, 1, 0, &mock_L1D.queues};

    std::vector test_instructions( 2*retire_bandwidth, champsim::test::instruction_with_ip(1) );

    uut.ROB.insert(std::end(uut.ROB), std::begin(test_instructions), std::end(test_instructions));

    auto old_rob_occupancy = std::size(uut.ROB);
    auto old_num_retired = uut.retired_count();

    WHEN("All instructions are executed") {
      uut.ROB[0].executed = COMPLETED;
      uut.ROB[1].executed = COMPLETED;

      uut.operate();
      mock_L1D.operate();

      THEN("The bandwidth of instructions are retired") {
        REQUIRE_THAT(uut.ROB, Catch::Matchers::SizeIs(old_rob_occupancy-static_cast<std::size_t>(uut.RETIRE_WIDTH)));
        REQUIRE(uut.retired_count() == old_num_retired+static_cast<std::size_t>(uut.RETIRE_WIDTH));
      }

      uut.operate();
      mock_L1D.operate();

      AND_THEN("The remaining instructions are retired") {
        REQUIRE(std::size(uut.ROB) == 0);
        REQUIRE(uut.retired_count() == old_num_retired+std::size(test_instructions));
      }
    }
  }
}
