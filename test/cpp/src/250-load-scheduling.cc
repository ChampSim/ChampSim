#include <catch.hpp>
#include "mocks.hpp"
#include "ooo_cpu.h"
#include "instr.h"

SCENARIO("The core issues loads only after its registers are finished") {
  GIVEN("A DISPATCH_BUFFER with a register RAW and memory source") {
    do_nothing_MRC mock_L1I, mock_L1D;
    O3_CPU uut{champsim::core_builder{}
      .fetch_queues(&mock_L1I.queues)
      .data_queues(&mock_L1D.queues)
      .dispatch_width(champsim::bandwidth::maximum_type{2})
      .rob_size(2)
      .lq_size(1)
    };

    auto producer = champsim::test::instruction_with_ip(champsim::address{2000});
    producer.destination_registers.push_back(1);
    producer.instr_id = 1;
    auto consumer = champsim::test::instruction_with_ip_and_source_memory(champsim::address{2004}, champsim::address{0xcafe0000});
    consumer.source_registers.push_back(1);
    consumer.instr_id = 2;

    uut.DISPATCH_BUFFER.push_back(producer);
    uut.DISPATCH_BUFFER.push_back(consumer);
    for (auto &instr : uut.DISPATCH_BUFFER)
      instr.ready_time = champsim::chrono::clock::time_point{};

    //auto old_cycle = uut.current_cycle();

    WHEN("The instructions are promoted to the ROB and given a cycle") {
      for (int i = 0; i < 2; ++i) {
        for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
          op->_operate();
      }

      THEN("The ROB has two entries") {
        REQUIRE(std::size(uut.ROB) == 2);
      }

      THEN("The load queue has one entry that has not been issued") {
        REQUIRE(std::count_if(std::begin(uut.LQ), std::end(uut.LQ), [](auto x){ return x.has_value(); }) == 1);
        REQUIRE(uut.LQ.at(0)->fetch_issued == false);
        REQUIRE(mock_L1D.packet_count() == 0);
      }

      AND_WHEN("The consumer is executed") {
        for (int i = 0; i < 10000 && !uut.ROB.back().executed; ++i) {
          for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
            op->_operate();
        }
        for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
          op->_operate();

        THEN("The load queue entry is issued") {
          auto found = std::find_if(std::begin(uut.LQ), std::end(uut.LQ), [](auto x){ return x.has_value(); });
          REQUIRE(found != std::end(uut.LQ));
          REQUIRE((*found)->fetch_issued == true);
          REQUIRE(mock_L1D.packet_count() == 1);
        }
      }
    }
  }
}


