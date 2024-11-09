#include <catch.hpp>
#include "mocks.hpp"
#include "bandwidth.h"
#include "defaults.hpp"
#include "ooo_cpu.h"
#include "instr.h"
#include "operable.h"
#include "champsim.h"

SCENARIO("The total latency is the sum of the stage latency") {
  GIVEN("An empty core with varying latencies") {
    const auto decode_latency = GENERATE(1u, 2u, 4u, 20u);
    const auto dispatch_latency= GENERATE(1u, 2u, 4u, 20u);
    const auto schedule_latency= GENERATE(1u, 2u, 4u, 20u);
    const auto execute_latency = GENERATE(1u, 2u, 4u, 20u);
    const auto num_instrs = GENERATE(1u, 2u, 5u);
    do_nothing_MRC mock_L1I, mock_L1D;

    O3_CPU uut{champsim::core_builder{}
        .ifetch_buffer_size(16)
        .decode_buffer_size(16)
        .dispatch_buffer_size(16)
        .register_file_size(128)
        .rob_size(16)
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
    std::vector test_instructions(num_instrs, champsim::test::instruction_with_ip(1));

    //only decode, schedule, dispatch, execute latency appears in O3_CPU
    auto cycle_complete_pipeline = decode_latency + schedule_latency + dispatch_latency + execute_latency + 5;
    auto expected_cycle_when_done = cycle_complete_pipeline + (unsigned int) (test_instructions.size()-1);

    WHEN("The instructions are added to the core")
    {
      const auto start_time = uut.current_time;
      uut.IFETCH_BUFFER.insert(std::end(uut.IFETCH_BUFFER), std::begin(test_instructions), std::end(test_instructions));

      for (unsigned int i=0; static_cast<std::size_t>(uut.num_retired) < std::size(test_instructions) && i < 2*expected_cycle_when_done; i++)
      {
        for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
          op->_operate();
      }

      THEN("The latency is the sum of the stage latencies")
      {
        REQUIRE(uut.current_time - start_time == expected_cycle_when_done*uut.clock_period);
      }
    }
  }
}

SCENARIO("The minimum specified core latency is 1") {
  GIVEN("An empty core with varying latencies") {
    const auto decode_latency = GENERATE(0u, 1u);
    const auto dispatch_latency= GENERATE(0u, 1u);
    const auto schedule_latency= GENERATE(0u, 1u);
    const auto execute_latency = GENERATE(0u, 1u);
    const auto num_instrs = GENERATE(1u, 2u, 5u);
    do_nothing_MRC mock_L1I, mock_L1D;

    O3_CPU uut{champsim::core_builder{}
        .ifetch_buffer_size(16)
        .decode_buffer_size(16)
        .dispatch_buffer_size(16)
        .register_file_size(128)
        .rob_size(16)
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
    std::vector test_instructions(num_instrs, champsim::test::instruction_with_ip(1));

    auto cycle_complete_pipeline = 9u; // Fixed value for both 0 and 1
    auto expected_cycle_when_done = cycle_complete_pipeline + (unsigned int) (test_instructions.size()-1);

    WHEN("The instructions are added to the core")
    {
      const auto start_time = uut.current_time;
      uut.IFETCH_BUFFER.insert(std::end(uut.IFETCH_BUFFER), std::begin(test_instructions), std::end(test_instructions));

      for (unsigned int i=0; static_cast<std::size_t>(uut.num_retired) < std::size(test_instructions) && i < 2*expected_cycle_when_done; i++)
      {
        for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
          op->_operate();
      }

      THEN("The latency is the sum of the stage latencies")
      {
        REQUIRE(uut.current_time == start_time + expected_cycle_when_done*uut.clock_period);
      }
    }
  }
}

