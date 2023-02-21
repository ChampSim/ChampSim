#include <catch.hpp>
#include "mocks.hpp"

#include "ooo_cpu.h"

namespace
{
ooo_model_instr inst(uint64_t addr)
{
    input_instr i;
    i.ip = addr;
    return ooo_model_instr{0, i};
}
}

SCENARIO("The fetch bandwidth limits the number of packets issued each cycle") {
  auto bandwidth = GENERATE(as<long>{}, 1,2,3,4,5);
  GIVEN("An instruction buffer with many different instructions") {
    constexpr std::array<uint64_t, 6> addrs{{0xdeadbeef, 0xbeefdead, 0xcafebabe, 0xbabecafe, 0xfeeddeaf, 0xdeaffeed}};

    do_nothing_MRC mock_L1I;
    do_nothing_MRC mock_L1D;
    O3_CPU uut{0, 1.0, {32, 8, {2}, {2}}, 64, 32, 32, 352, 128, 72, 2, 2, 2, 128, 1, 2, 2, 1, 1, 1, 1, 1, 0, nullptr, &mock_L1I.queues, bandwidth, &mock_L1D.queues, 1, O3_CPU::bbranchDbimodal, O3_CPU::tbtbDbasic_btb};

    std::array<champsim::operable*,3> elements = {&uut, &mock_L1I, &mock_L1D};

    std::for_each(std::begin(addrs), std::end(addrs), [&](auto x){
      uut.IFETCH_BUFFER.push_back(::inst(x));
      uut.IFETCH_BUFFER.back().dib_checked = COMPLETED;
    });

    WHEN("The fetch operates") {
      for (auto x : elements)
        x->_operate();

      THEN("The bandwidth number of packets are issued") {
        REQUIRE(mock_L1I.packet_count() == (long unsigned) bandwidth);
      }
    }
  }
}
