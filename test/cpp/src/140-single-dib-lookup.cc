#include <catch.hpp>
#include "mocks.hpp"
#include "ooo_cpu.h"
#include "instr.h"

SCENARIO("A late-added instruction does not miss the IFB") {
  GIVEN("An IFETCH_BUFFER with one inflight instruction") {
    release_MRC mock_L1I;
    do_nothing_MRC mock_L1D;
    O3_CPU uut{0, 1.0, {32, 8, {2}, {2}}, 64, 32, 32, 352, 128, 72, 2, 2, 2, 128, 1, 2, 2, 1, 1, 1, 1, 1, 0, nullptr, &mock_L1I.queues, 1, &mock_L1D.queues, 1, O3_CPU::bbranchDbimodal, O3_CPU::tbtbDbasic_btb};

    std::array<champsim::operable*, 3> elements{{&uut, &mock_L1I, &mock_L1D}};

    uut.IFETCH_BUFFER.push_back(champsim::test::instruction_with_ip(0xdeadbeef));
    for (auto &instr : uut.IFETCH_BUFFER)
      instr.event_cycle = uut.current_cycle;

    for (int i = 0; i < 3; ++i) {
      for (auto op : elements)
        op->_operate();
    }

    CHECK(mock_L1I.packet_count() == 1);

    WHEN("A new instruction is added, and the first request returns") {
      mock_L1I.release(champsim::address{0xdeadbeef});

      uut.IFETCH_BUFFER.push_back(champsim::test::instruction_with_ip(0xdeadbeee)); // same cache line as first instruction

      for (int i = 0; i < 3; ++i) {
          for (auto op : elements)
              op->_operate();
      }

      THEN("The IFETCH_BUFFER still has one member") {
        REQUIRE(std::size(uut.IFETCH_BUFFER) == 1);
        REQUIRE(uut.IFETCH_BUFFER.front().ip == champsim::address{0xdeadbeee});
      }
    }
  }
}

