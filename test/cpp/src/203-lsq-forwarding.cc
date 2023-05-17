#include <catch.hpp>

#include "reorder_buffer.h"
#include "instr.h"
#include "mocks.hpp"

SCENARIO("ROB scheduling does not issue reads for instructions that will forward") {
  GIVEN("An empty ROB") {
    constexpr unsigned rob_size = 352;
    constexpr unsigned lq_size = 128;
    constexpr unsigned sq_size = 72;

    do_nothing_MRC mock_L1D;
    champsim::reorder_buffer uut{0, rob_size, lq_size, sq_size, 128, 1, 2, 2, 1, 1, 1, 1, 0, &mock_L1D.queues};

    WHEN("A memory RAW dependency is added to the ROB") {
      auto producer_instr = champsim::test::instruction_with_memory({0xdeadbeef}, {0x11111111});
      auto consumer_instr = champsim::test::instruction_with_memory({0x22222222}, {0xdeadbeef});

      producer_instr.instr_id = 1;
      consumer_instr.instr_id = 2;

      uut.push_back(producer_instr);
      uut.push_back(consumer_instr);

      for (std::size_t i = 0; i < 10; ++i) {
        uut.operate();
        mock_L1D.operate();
      }

      THEN("One request is issued") {
        REQUIRE_THAT(mock_L1D.addresses, Catch::Matchers::SizeIs(3)
            && Catch::Matchers::Contains(0x11111111)
            && Catch::Matchers::Contains(0x22222222)
            && Catch::Matchers::Contains(0xdeadbeef)
            );
      }
    }
  }
}
