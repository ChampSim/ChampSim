#include <catch.hpp>
#include "mocks.hpp"
#include "instr.h"

#include <array>

#include "ooo_cpu.h"
#include "defaults.hpp"

SCENARIO("Blocks that hit the DIB are removed from fetch candidacy") {
  GIVEN("A core that has decoded one instruction") {
    do_nothing_MRC mock_L1I, mock_L1D;

    O3_CPU uut{champsim::core_builder{champsim::defaults::default_core}
      .fetch_queues(&mock_L1I.queues)
        .data_queues(&mock_L1D.queues)
        .decode_latency(10)
    };
    uut.warmup = false;

    auto seed_instr = champsim::test::instruction_with_ip(champsim::address{0xfeed0040});

    uut.IFETCH_BUFFER.push_back(seed_instr);

    for (auto i = 0; i < 100; i++) {
      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();
    }

    mock_L1I.addresses.clear();
    WHEN("The core encounters that instruction surrounded by two instructions of the same block (BAB)") {
      auto test_instr_first = champsim::test::instruction_with_ip(champsim::address{0xbeefbeef});
      auto test_instr_second = champsim::test::instruction_with_ip(champsim::address{0xbeefbee0});

      uut.IFETCH_BUFFER.push_back(test_instr_first);
      uut.IFETCH_BUFFER.push_back(seed_instr);
      uut.IFETCH_BUFFER.push_back(test_instr_second);

      for (auto i = 0; i < 100; i++) {
        for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
          op->_operate();
      }

      THEN("Only one fetch request is issued") {
        REQUIRE_THAT(mock_L1I.addresses, Catch::Matchers::RangeEquals(std::array{test_instr_first.ip,test_instr_second.ip}));
      }
    }
  }
}
