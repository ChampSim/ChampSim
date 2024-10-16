#include <catch.hpp>

#include "stats_printer.h"
#include "core_stats.h"

TEST_CASE("An empty core stats prints zero") {
  cpu_stats given{};
  given.name = "test_cpu";

  std::vector<std::string> expected{
    "test_cpu cumulative IPC: - instructions: 0 cycles: 0",
    "test_cpu Branch Prediction Accuracy: -% MPKI: - Average ROB Occupancy at Mispredict: -",
    "Branch type MPKI",
    "BRANCH_DIRECT_JUMP: -",
    "BRANCH_INDIRECT: -",
    "BRANCH_CONDITIONAL: -",
    "BRANCH_DIRECT_CALL: -",
    "BRANCH_INDIRECT_CALL: -",
    "BRANCH_RETURN: -"
  };

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("The number of instructions and cycles modifies the IPC") {
  cpu_stats given{};
  given.name = "test_cpu";
  given.begin_instrs = 0;
  given.begin_cycles = 0;
  given.end_instrs = 100;
  given.end_cycles = 50;

  std::vector<std::string> expected{
    "test_cpu cumulative IPC: 2 instructions: 100 cycles: 50",
    "test_cpu Branch Prediction Accuracy: -% MPKI: 0 Average ROB Occupancy at Mispredict: -",
    "Branch type MPKI",
    "BRANCH_DIRECT_JUMP: 0",
    "BRANCH_INDIRECT: 0",
    "BRANCH_CONDITIONAL: 0",
    "BRANCH_DIRECT_CALL: 0",
    "BRANCH_INDIRECT_CALL: 0",
    "BRANCH_RETURN: 0"
  };

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("The number of mispredictions modifies the MPKI") {
  auto num_misses = 255;
  auto [line_index, miss_type, expected_line] = GENERATE(as<std::tuple<std::size_t, branch_type, std::string>>{},
      std::tuple{3, branch_type::BRANCH_DIRECT_JUMP, "BRANCH_DIRECT_JUMP: 255"},
      std::tuple{4, branch_type::BRANCH_INDIRECT, "BRANCH_INDIRECT: 255"},
      std::tuple{5, branch_type::BRANCH_CONDITIONAL, "BRANCH_CONDITIONAL: 255"},
      std::tuple{6, branch_type::BRANCH_DIRECT_CALL, "BRANCH_DIRECT_CALL: 255"},
      std::tuple{7, branch_type::BRANCH_INDIRECT_CALL, "BRANCH_INDIRECT_CALL: 255"},
      std::tuple{8, branch_type::BRANCH_RETURN, "BRANCH_RETURN: 255"}
  );

  cpu_stats given{};
  given.name = "test_cpu";
  given.begin_instrs = 0;
  given.begin_cycles = 0;
  given.end_instrs = 1000;
  given.end_cycles = 500;
  given.total_branch_types.set(miss_type, 2*num_misses);
  given.branch_type_misses.set(miss_type, num_misses);

  std::vector<std::string> expected{
    "test_cpu cumulative IPC: 2 instructions: 1000 cycles: 500",
    "test_cpu Branch Prediction Accuracy: 50% MPKI: 255 Average ROB Occupancy at Mispredict: 0",
    "Branch type MPKI",
    "BRANCH_DIRECT_JUMP: 0",
    "BRANCH_INDIRECT: 0",
    "BRANCH_CONDITIONAL: 0",
    "BRANCH_DIRECT_CALL: 0",
    "BRANCH_INDIRECT_CALL: 0",
    "BRANCH_RETURN: 0"
  };
  expected.at(line_index) = expected_line;

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("The ROB occupancy modifies the flush penalty") {
  auto num_misses = 100;

  cpu_stats given{};
  given.name = "test_cpu";
  given.begin_instrs = 0;
  given.begin_cycles = 0;
  given.end_instrs = 1000;
  given.end_cycles = 500;
  given.total_branch_types.set(branch_type::BRANCH_DIRECT_JUMP, 2*num_misses);
  given.branch_type_misses.set(branch_type::BRANCH_DIRECT_JUMP, num_misses);
  given.total_rob_occupancy_at_branch_mispredict = (uint64_t)(10*num_misses);

  std::vector<std::string> expected{
    "test_cpu cumulative IPC: 2 instructions: 1000 cycles: 500",
    "test_cpu Branch Prediction Accuracy: 50% MPKI: 100 Average ROB Occupancy at Mispredict: 10",
    "Branch type MPKI",
    "BRANCH_DIRECT_JUMP: 100",
    "BRANCH_INDIRECT: 0",
    "BRANCH_CONDITIONAL: 0",
    "BRANCH_DIRECT_CALL: 0",
    "BRANCH_INDIRECT_CALL: 0",
    "BRANCH_RETURN: 0"
  };

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}
