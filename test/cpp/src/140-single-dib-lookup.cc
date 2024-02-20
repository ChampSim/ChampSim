#include <catch.hpp>
#include "mocks.hpp"
#include "ooo_cpu.h"
#include "instr.h"

SCENARIO("A late-added instruction does not miss the IFB") {
  GIVEN("An IFETCH_BUFFER with one inflight instruction") {
    release_MRC mock_L1I;
    do_nothing_MRC mock_L1D;
    O3_CPU uut{champsim::core_builder{}
      .dib_window(4)
      .ifetch_buffer_size(2)
      .l1i_bandwidth(champsim::bandwidth::maximum_type{10})
      .l1d_bandwidth(champsim::bandwidth::maximum_type{10})
      .fetch_queues(&mock_L1I.queues)
      .data_queues(&mock_L1D.queues)
    };

    std::array<champsim::operable*, 3> elements{{&uut, &mock_L1I, &mock_L1D}};

    uut.IFETCH_BUFFER.push_back(champsim::test::instruction_with_ip(0xdeadbeef));
    for (auto &instr : uut.IFETCH_BUFFER) {
      instr.ready_time = champsim::chrono::clock::time_point{};
      instr.dib_checked = true;
      //instr.dib_checked = COMPLETED;
    }

    for (int i = 0; i < 10; ++i) {
      for (auto op : elements)
        op->_operate();
    }

    THEN("The instruction issues a fetch") {
      REQUIRE(mock_L1I.packet_count() == 1);
      REQUIRE(uut.IFETCH_BUFFER.front().ip == champsim::address{0xdeadbeef});
    }

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

