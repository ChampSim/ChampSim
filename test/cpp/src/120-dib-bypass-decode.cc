#include <catch.hpp>
#include "mocks.hpp"
#include "bandwidth.h"
#include "defaults.hpp"
#include "ooo_cpu.h"
#include "instr.h"
#include "operable.h"
#include "champsim.h"
#include <algorithm>

SCENARIO("The same instruction hits the DIB on the second time") {
  GIVEN("A core that has seen a sequence of instructions") {
    const unsigned int fetch_latency = 3;
    const unsigned int decode_latency= GENERATE(1u, 2u, 4, 20u);
    const unsigned int dispatch_latency=2;
    const unsigned int schedule_latency=2;
    const unsigned int execute_latency =2;
    do_nothing_MRC mock_L1I{fetch_latency}, mock_L1D;

    O3_CPU uut{champsim::core_builder{champsim::defaults::default_core}
      .fetch_queues(&mock_L1I.queues)
        .data_queues(&mock_L1D.queues)
        .decode_latency(decode_latency)
        .dispatch_latency(dispatch_latency)
        .schedule_latency(schedule_latency)
        .execute_latency(execute_latency)
        .execute_width(champsim::bandwidth::maximum_type{1})
        .decode_width(champsim::bandwidth::maximum_type{1})
        .dispatch_width(champsim::bandwidth::maximum_type{1})
        .fetch_width(champsim::bandwidth::maximum_type{1})
        .retire_width(champsim::bandwidth::maximum_type{1})
    };
    uut.warmup = false;
    std::vector test_instructions(1, champsim::test::instruction_with_ip(1));

    uut.IFETCH_BUFFER.insert(std::end(uut.IFETCH_BUFFER), std::begin(test_instructions), std::end(test_instructions));

    const std::size_t expected_cycles = decode_latency + dispatch_latency + schedule_latency + execute_latency + 5;
    for (std::size_t i=0; static_cast<std::size_t>(uut.num_retired) < std::size(test_instructions) && i < 5*expected_cycles; i++)
    {
      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();
    }

    WHEN("The instructions pass through the core a second time")
    {
      auto begin_second_time = uut.current_time;

      uut.IFETCH_BUFFER.insert(std::end(uut.IFETCH_BUFFER), std::begin(test_instructions), std::end(test_instructions));

      for (std::size_t i=0; static_cast<std::size_t>(uut.num_retired) < 2*std::size(test_instructions) && i < 2*expected_cycles; i++)
      {
        for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
          op->_operate();
      }
      auto end_second_time = uut.current_time;

      THEN("The latency is the sum of the stage latencies")
      {
        REQUIRE(static_cast<std::size_t>(uut.num_retired) == 2*std::size(test_instructions));

        // A hit in the DIB should skip the decode latency
        REQUIRE((end_second_time - begin_second_time)/uut.clock_period == static_cast<long>((expected_cycles - decode_latency)));
      }
    }
  }
}
