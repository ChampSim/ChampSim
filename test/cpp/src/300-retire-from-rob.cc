#include <catch.hpp>
#include "mocks.hpp"
#include "ooo_cpu.h"
#include "instr.h"

SCENARIO("An empty ROB does not retire any instructions") {
  GIVEN("An empty ROB") {
    do_nothing_MRC mock_L1I, mock_L1D;
    constexpr long retire_bandwidth = 1;
    O3_CPU uut{champsim::core_builder{}
      .retire_width(champsim::bandwidth::maximum_type{retire_bandwidth})
      .fetch_queues(&mock_L1I.queues)
      .data_queues(&mock_L1D.queues)
    };

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

SCENARIO("A completed instruction can be retired") {
  GIVEN("A ROB with a single instruction") {
    do_nothing_MRC mock_L1I, mock_L1D;
    constexpr long retire_bandwidth = 1;
    O3_CPU uut{champsim::core_builder{}
      .retire_width(champsim::bandwidth::maximum_type{retire_bandwidth})
      .fetch_queues(&mock_L1I.queues)
      .data_queues(&mock_L1D.queues)
    };

    uut.ROB.push_back(champsim::test::instruction_with_ip(1));

    auto old_rob_occupancy = std::size(uut.ROB);
    auto old_num_retired = uut.num_retired;

    WHEN("The instruction is not completed") {
      uut.ROB.front().completed = false;
      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();

      THEN("The number of retired instructions stays the same") {
        REQUIRE(std::size(uut.ROB) == old_rob_occupancy);
        REQUIRE(uut.num_retired == old_num_retired);
      }
    }

    WHEN("The instruction has been completed") {
      uut.ROB.front().completed = true;
      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();

      THEN("The instruction is retired") {
        REQUIRE(std::size(uut.ROB) == 0);
        REQUIRE(uut.num_retired == old_num_retired+1);
      }
    }
  }
}

SCENARIO("Completed instructions are retired in order") {
  GIVEN("A ROB with two instructions") {
    do_nothing_MRC mock_L1I, mock_L1D;
    constexpr long retire_bandwidth = 2;
    O3_CPU uut{champsim::core_builder{}
      .retire_width(champsim::bandwidth::maximum_type{retire_bandwidth})
      .fetch_queues(&mock_L1I.queues)
      .data_queues(&mock_L1D.queues)
    };

    std::vector test_instructions( retire_bandwidth, champsim::test::instruction_with_ip(1) );

    uut.ROB.insert(std::end(uut.ROB), std::begin(test_instructions), std::end(test_instructions));

    auto old_rob_occupancy = std::size(uut.ROB);
    auto old_num_retired = uut.num_retired;

    WHEN("The second instruction is completed") {
      uut.ROB[0].completed = false;
      uut.ROB[1].completed = true;

      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();

      THEN("No instructions are retired") {
        REQUIRE(std::size(uut.ROB) == old_rob_occupancy);
        REQUIRE(uut.num_retired == old_num_retired);
      }
    }

    WHEN("Both instructions are completed") {
      uut.ROB[0].completed = true;
      uut.ROB[1].completed = true;

      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();

      THEN("Both instructions are retired") {
        REQUIRE(std::size(uut.ROB) == 0);
        REQUIRE(uut.num_retired == old_num_retired+retire_bandwidth);
      }
    }
  }
}

SCENARIO("The retire bandwidth limits the number of retirements per cycle") {
  GIVEN("A ROB with twice as many instructions as retire bandwidth") {
    do_nothing_MRC mock_L1I, mock_L1D;
    constexpr long retire_bandwidth = 1;
    constexpr long num_instrs = 2 * retire_bandwidth;
    O3_CPU uut{champsim::core_builder{}
      .retire_width(champsim::bandwidth::maximum_type{retire_bandwidth})
      .fetch_queues(&mock_L1I.queues)
      .data_queues(&mock_L1D.queues)
    };

    std::vector test_instructions( num_instrs, champsim::test::instruction_with_ip(1) );

    uut.ROB.insert(std::end(uut.ROB), std::begin(test_instructions), std::end(test_instructions));

    auto old_rob_occupancy = std::size(uut.ROB);
    auto old_num_retired = uut.num_retired;

    WHEN("All instructions are completed") {
      uut.ROB[0].completed = true;
      uut.ROB[1].completed = true;

      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();

      THEN("The bandwidth of instructions are retired") {
        REQUIRE_THAT(uut.ROB, Catch::Matchers::SizeIs(old_rob_occupancy-retire_bandwidth));
        REQUIRE(uut.num_retired == old_num_retired+retire_bandwidth);
      }

      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();

      AND_THEN("The remaining instructions are retired") {
        REQUIRE(std::size(uut.ROB) == 0);
        REQUIRE(uut.num_retired == old_num_retired+num_instrs);
      }
    }
  }
}
