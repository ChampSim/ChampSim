#include <array>
#include <catch.hpp>

#include "defaults.hpp"
#include "instr.h"
#include "mocks.hpp"
#include "ooo_cpu.h"

SCENARIO("Blocks that hit the DIB are removed from fetch candidacy")
{
  GIVEN("A core that has decoded one instruction")
  {
    do_nothing_MRC mock_L1I, mock_L1D;

    O3_CPU uut{champsim::modules::ModuleBuilder{"t151_core", "DEFAULT_CORE", test_core_defaults("t151_core_ws")}.add_parameter("fetch_queues", static_cast<champsim::modules::channel_module*>(&mock_L1I.queues)).add_parameter("data_queues", static_cast<champsim::modules::channel_module*>(&mock_L1D.queues)).add_parameter("decode_latency", static_cast<unsigned>(10))};
    uut.begin_phase(false);

    auto seed_instr = champsim::test::instruction_with_ip(champsim::address{0xfeed0040});

    uut.IFETCH_BUFFER.push_back(seed_instr);

    for (auto i = 0; i < 100; i++) {
      for (auto op : std::array<champsim::operable*, 3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();
    }

    mock_L1I.addresses.clear();
    WHEN("The core encounters that instruction surrounded by two instructions of the same block (BAB)")
    {
      auto test_instr_first = champsim::test::instruction_with_ip(champsim::address{0xbeefbeef});
      auto test_instr_second = champsim::test::instruction_with_ip(champsim::address{0xbeefbee0});

      uut.IFETCH_BUFFER.push_back(test_instr_first);
      uut.IFETCH_BUFFER.push_back(seed_instr);
      uut.IFETCH_BUFFER.push_back(test_instr_second);

      for (auto i = 0; i < 100; i++) {
        for (auto op : std::array<champsim::operable*, 3>{{&uut, &mock_L1I, &mock_L1D}})
          op->_operate();
      }

      THEN("Only one fetch request is issued")
      {
        REQUIRE_THAT(mock_L1I.addresses, Catch::Matchers::RangeEquals(std::array{test_instr_first.ip, test_instr_second.ip}));
      }
    }
  }
}
