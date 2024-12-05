#include <catch.hpp>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <vector>
#include "matchers.hpp"

#include "tracereader.h"

TEST_CASE("A single tracereader produces monotonically increasing instruction IDs") {
  champsim::tracereader uut{[](){ return ooo_model_instr{0, input_instr{}}; }};

  std::vector<std::invoke_result_t<decltype(uut)>> generated_instrs{};
  std::generate_n(std::back_inserter(generated_instrs), 10, std::ref(uut));
  std::vector<uint64_t> ids{};
  std::transform(std::begin(generated_instrs), std::end(generated_instrs), std::back_inserter(ids), [](const auto& x){ return x.instr_id; });

  REQUIRE_THAT(ids, champsim::test::MonotonicallyIncreasingMatcher{});
}

TEST_CASE("Two tracereaders produce monotonically increasing instruction IDs") {
  champsim::tracereader uuta{[](){ return ooo_model_instr{0, input_instr{}}; }};
  champsim::tracereader uutb{[](){ return ooo_model_instr{0, input_instr{}}; }};

  std::vector<std::invoke_result_t<decltype(uuta)>> generated_instrs{};
  std::generate_n(std::back_inserter(generated_instrs), 10, std::ref(uuta));
  std::generate_n(std::back_inserter(generated_instrs), 10, std::ref(uutb));
  std::vector<uint64_t> ids{};
  std::transform(std::begin(generated_instrs), std::end(generated_instrs), std::back_inserter(ids), [](const auto& x){ return x.instr_id; });

  REQUIRE_THAT(ids, champsim::test::MonotonicallyIncreasingMatcher{});
}
