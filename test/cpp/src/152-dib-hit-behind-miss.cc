#include <array>
#include <catch.hpp>

#include "defaults.hpp"
#include "instr.h"
#include "mocks.hpp"
#include "ooo_cpu.h"

SCENARIO("An instruction that hits the DIB does not fetch from the L1I while it waits behind a miss")
{
  GIVEN("A core that has decoded one instruction")
  {
    release_MRC mock_L1I;
    do_nothing_MRC mock_L1D;

    O3_CPU uut{champsim::modules::ModuleBuilder{"t152_core", "DEFAULT_CORE", champsim::defaults::default_core()}
                   .add_parameter("fetch_queues", static_cast<champsim::modules::channel_module*>(&mock_L1I.queues))
                   .add_parameter("data_queues", static_cast<champsim::modules::channel_module*>(&mock_L1D.queues))};
    uut.warmup = false;

    std::array<champsim::operable*, 3> elements{{&uut, &mock_L1I, &mock_L1D}};
    auto run = [&](int cycles) {
      for (auto i = 0; i < cycles; i++) {
        for (auto op : elements)
          op->_operate();
      }
    };

    auto seed_instr = champsim::test::instruction_with_ip(champsim::address{0xfeed0040});
    uut.IFETCH_BUFFER.push_back(seed_instr);
    run(10);
    mock_L1I.release_all();
    run(100);

    const auto fetches_before = mock_L1I.packet_count();

    WHEN("The core encounters that instruction behind an instruction from another block whose fetch has not returned")
    {
      auto miss_instr = champsim::test::instruction_with_ip(champsim::address{0xbeef0000});

      uut.IFETCH_BUFFER.push_back(miss_instr);
      uut.IFETCH_BUFFER.push_back(seed_instr);

      run(100);

      THEN("Only the missing instruction is fetched") { REQUIRE(mock_L1I.packet_count() - fetches_before == 1); }

      AND_WHEN("The fetch returns")
      {
        mock_L1I.release_all();
        run(100);

        THEN("Both instructions retire without another fetch")
        {
          REQUIRE(uut.num_retired == 3);
          REQUIRE(mock_L1I.packet_count() - fetches_before == 1);
        }
      }
    }

    WHEN("The core encounters that instruction several times behind the same outstanding miss")
    {
      auto miss_instr = champsim::test::instruction_with_ip(champsim::address{0xbeef0000});

      uut.IFETCH_BUFFER.push_back(miss_instr);
      for (auto i = 0; i < 4; i++)
        uut.IFETCH_BUFFER.push_back(seed_instr);

      run(100);

      THEN("Only the missing instruction is fetched") { REQUIRE(mock_L1I.packet_count() - fetches_before == 1); }
    }
  }
}
