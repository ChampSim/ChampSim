#include <catch.hpp>

#include "instr.h"
#include "mocks.hpp"
#include "ooo_cpu.h"
#include "defaults.hpp"

SCENARIO("The fetch bandwidth limits the number of packets issued each cycle")
{
  auto bandwidth = GENERATE(as<long>{}, 1, 2, 3, 4, 5);
  GIVEN("An instruction buffer with many different instructions")
  {
    constexpr std::array<uint64_t, 6> addrs{{0xdeadbeef, 0xbeefdead, 0xcafebabe, 0xbabecafe, 0xfeeddeaf, 0xdeaffeed}};

    do_nothing_MRC mock_L1I;
    do_nothing_MRC mock_L1D;
    O3_CPU uut{
        champsim::modules::ModuleBuilder{"t150_core", "DEFAULT_CORE", test_core_defaults("t150_core_ws")}.add_parameter("fetch_queues", static_cast<champsim::modules::channel_module*>(&mock_L1I.queues)).add_parameter("data_queues", static_cast<champsim::modules::channel_module*>(&mock_L1D.queues)).add_parameter("l1i_bandwidth", champsim::bandwidth::maximum_type{bandwidth})};

    std::array<champsim::operable*, 3> elements = {&uut, &mock_L1I, &mock_L1D};

    std::for_each(std::begin(addrs), std::end(addrs), [&](auto x) {
      uut.IFETCH_BUFFER.push_back(champsim::test::instruction_with_ip(x));
      uut.IFETCH_BUFFER.back().dib_checked = true;
    });

    WHEN("The fetch operates")
    {
      for (auto x : elements)
        x->_operate();

      THEN("The bandwidth number of packets are issued") { REQUIRE(mock_L1I.packet_count() == (long unsigned)bandwidth); }
    }
  }
}
