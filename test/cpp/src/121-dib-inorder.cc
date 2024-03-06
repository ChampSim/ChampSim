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

auto block_sequence_generator(champsim::block_number blk) {
  return [blk]() mutable { return champsim::address{blk++}; };
}

auto instr_id_sequence_generator(uint64_t id) {
  return [id]() mutable { return id++; };
}

auto instruction_generator(champsim::block_number base_ip, uint64_t base_id) {
  return [ip_gen = block_sequence_generator(base_ip), id_gen = instr_id_sequence_generator(base_id)]() mutable {
    ooo_model_instr instr{champsim::test::instruction_with_ip(ip_gen())};
    instr.instr_id = id_gen();
    return instr;
  };
}

auto instruction_smem_generator(champsim::block_number base_ip, champsim::block_number base_smem, uint64_t base_id) {
  return [ip_gen = block_sequence_generator(base_ip), smem_gen = block_sequence_generator(base_smem), id_gen = instr_id_sequence_generator(base_id)]() mutable {
    ooo_model_instr instr{champsim::test::instruction_with_ip_and_source_memory(ip_gen(), smem_gen())};
    instr.instr_id = id_gen();
    return instr;
  };
}

SCENARIO("Instructions that hit the DIB do not reorder ahead of instructions that miss") {
  const std::size_t num_seeds = 3;
  const std::size_t num_additional_tests = 2;
  GIVEN("A core that has decode a few instructions") {
    do_nothing_MRC mock_L1I, mock_L1D;

    O3_CPU uut{champsim::core_builder{champsim::defaults::default_core}
      .fetch_queues(&mock_L1I.queues)
        .data_queues(&mock_L1D.queues)
        .decode_latency(10)
    };
    uut.warmup = false;

    const champsim::block_number seed_base_addr{0xfeed001};

    // Create a sequence of seed instructions
    std::vector<ooo_model_instr> seed_instructions{};
    std::generate_n(std::back_inserter(seed_instructions), num_seeds, instruction_generator(seed_base_addr, 1));

    uut.IFETCH_BUFFER.insert(std::end(uut.IFETCH_BUFFER), std::begin(seed_instructions), std::end(seed_instructions));

    for (auto i = 0; i < 100; i++) {
      for (auto op : std::array<champsim::operable*,3>{{&uut, &mock_L1I, &mock_L1D}})
        op->_operate();
    }

    WHEN("A new instruction is issued, followed by the instructions already seen") {
      const auto num_tests = num_seeds + num_additional_tests;
      const auto test_base_addr = seed_base_addr - num_additional_tests;
      const champsim::block_number test_store_base_addr{0xbeef000}; // Use the store buffer to examine the order in which the instructions are retired

      std::vector<ooo_model_instr> test_instructions{};
      std::generate_n(std::back_inserter(test_instructions), num_tests, instruction_smem_generator(test_base_addr, test_store_base_addr, 11));

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
