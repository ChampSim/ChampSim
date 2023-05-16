#include <catch.hpp>

#include "reorder_buffer.h"
#include "instr.h"
#include "mocks.hpp"

SCENARIO("The LQ can issue requests") {
  GIVEN("An empty ROB") {
    constexpr unsigned rob_size = 352;
    constexpr unsigned lq_size = 128;
    constexpr unsigned sq_size = 72;

    do_nothing_MRC mock_L1D;
    champsim::reorder_buffer uut{0, rob_size, lq_size, sq_size, 128, 1, 2, 2, 1, 1, 1, 1, 0, &mock_L1D.queues};

    WHEN("An instruction with one memory source is inserted") {
      auto candidate_instr = champsim::test::instruction_with_source_memory(0xdeadbeef);
      uut.push_back(candidate_instr);

      for (std::size_t i = 0; i < 10; ++i) {
        uut.operate();
        mock_L1D.operate();
      }

      THEN("One request is issued") {
        REQUIRE_THAT(mock_L1D.addresses, Catch::Matchers::SizeIs(1) && Catch::Matchers::Contains(0xdeadbeef));
      }
    }
  }
}

SCENARIO("The SQ can issue requests") {
  GIVEN("An empty ROB") {
    constexpr unsigned rob_size = 352;
    constexpr unsigned lq_size = 128;
    constexpr unsigned sq_size = 72;

    do_nothing_MRC mock_L1D;
    champsim::reorder_buffer uut{0, rob_size, lq_size, sq_size, 128, 1, 2, 2, 1, 1, 1, 1, 0, &mock_L1D.queues};

    WHEN("An instruction with one memory destination is inserted") {
      auto candidate_instr = champsim::test::instruction_with_destination_memory(0xdeadbeef);
      uut.push_back(candidate_instr);

      for (std::size_t i = 0; i < 10; ++i) {
        uut.operate();
        mock_L1D.operate();
      }

      THEN("One request is issued") {
        REQUIRE_THAT(mock_L1D.addresses, Catch::Matchers::SizeIs(1) && Catch::Matchers::Contains(0xdeadbeef));
      }
    }
  }
}

SCENARIO("The LQ and SQ can issue simulataneous requests") {
  GIVEN("An empty ROB") {
    constexpr unsigned rob_size = 352;
    constexpr unsigned lq_size = 128;
    constexpr unsigned sq_size = 72;

    do_nothing_MRC mock_L1D;
    champsim::reorder_buffer uut{0, rob_size, lq_size, sq_size, 128, 1, 2, 2, 1, 1, 1, 1, 0, &mock_L1D.queues};

    WHEN("An instruction with one memory source and one destination is inserted") {
      auto candidate_instr = champsim::test::instruction_with_memory({0xdeadbeef}, {0xcafebabe});
      uut.push_back(candidate_instr);

      for (std::size_t i = 0; i < 10; ++i) {
        uut.operate();
        mock_L1D.operate();
      }

      THEN("One request is issued") {
        REQUIRE_THAT(mock_L1D.addresses, Catch::Matchers::SizeIs(2) && Catch::Matchers::Contains(0xdeadbeef) && Catch::Matchers::Contains(0xcafebabe));
      }
    }
  }
}

