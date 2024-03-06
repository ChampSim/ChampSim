#include <catch.hpp>
#include "mocks.hpp"
#include "bandwidth.h"
#include "defaults.hpp"
#include "ooo_cpu.h"
#include "instr.h"
#include "operable.h"
#include "champsim.h"
#include <algorithm>

struct IsSortedMatcher : Catch::Matchers::MatcherGenericBase {
  template<typename Range>
  bool match(Range const& range) const {
    return std::is_sorted(std::cbegin(range), std::cend(range));
  }

  std::string describe() const override {
    return "is sorted";
  }
};

SCENARIO("Hits to the DIB issue to the ROB in order") {
  GIVEN("A core that has decode a few instructions") {
    do_nothing_MRC mock_L1I, mock_L1D;

    O3_CPU uut{champsim::core_builder{champsim::defaults::default_core}
      .fetch_queues(&mock_L1I.queues)
        .data_queues(&mock_L1D.queues)
        .decode_latency(10)
    };
    uut.warmup = false;

    const std::size_t num_seeds = 2;
    const uint64_t seed_base_addr = 0xfeed0040;

    // Create a sequence of seed instructions
    std::vector<champsim::address> seed_ips{};
    std::generate_n(std::back_inserter(seed_ips), num_seeds, [block=champsim::block_number{seed_base_addr}]() mutable { return champsim::address{block++}; });
    std::vector<ooo_model_instr> seed_instructions{};
    std::transform(std::cbegin(seed_ips), std::cend(seed_ips), std::back_inserter(seed_instructions), [](auto x){ return champsim::test::instruction_with_ip(x); });
    std::for_each(std::begin(seed_instructions), std::end(seed_instructions), [id = uint64_t{1}](auto& instr) mutable { instr.instr_id = id++; });

    uut.IFETCH_BUFFER.insert(std::end(uut.IFETCH_BUFFER), std::begin(seed_instructions), std::end(seed_instructions));

    for (auto i = 0; i < 100; i++) {
      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();
    }

    WHEN("A new instruction is issued, followed by the instructions already seen") {
      const std::size_t num_tests = num_seeds + 1;
      const uint64_t test_base_addr = seed_base_addr - BLOCK_SIZE;
      const uint64_t test_store_base_addr = 0xbeef0000;

      std::vector<champsim::address> test_ips{};
      std::vector<champsim::address> test_store_addresses_to_check{}; // Use the store buffer to examine the order in which the instructions are retired

      std::generate_n(std::back_inserter(test_ips), num_tests, [block=champsim::block_number{test_base_addr}]() mutable { return champsim::address{block++}; });
      std::generate_n(std::back_inserter(test_store_addresses_to_check), num_tests, [block=champsim::block_number{test_store_base_addr}]() mutable { return champsim::address{block++}; });

      std::vector<ooo_model_instr> test_instructions{};
      std::transform(std::cbegin(test_ips), std::cend(test_ips), std::cbegin(test_store_addresses_to_check), std::back_inserter(test_instructions), champsim::test::instruction_with_ip_and_source_memory);
      std::for_each(std::begin(test_instructions), std::end(test_instructions), [id = uint64_t{11}](auto& instr) mutable { instr.instr_id = id++; });

      uut.IFETCH_BUFFER.insert(std::end(uut.IFETCH_BUFFER), std::begin(test_instructions), std::end(test_instructions));

      for (auto i = 0; i < 100; i++) {
        for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
          op->_operate();
      }

      THEN("The instructions are dispatched in order") {
        REQUIRE_THAT(mock_L1D.addresses, IsSortedMatcher{} && Catch::Matchers::SizeIs(num_tests));
      }
    }
  }
}
