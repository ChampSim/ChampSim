#include <catch.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_templated.hpp>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <vector>

#include "tracereader.h"

struct MonotonicallyIncreasingMatcher : Catch::Matchers::MatcherGenericBase {
    template<typename Range>
    bool match(Range const& range) const {
        return std::adjacent_find(std::begin(range), std::end(range), std::greater_equal<typename Range::value_type>{}) == std::end(range);
    }

    std::string describe() const override {
        return "Increases monotonically";
    }
};

TEST_CASE("A single tracereader produces monotonically increasing instruction IDs") {
  champsim::tracereader uut{[](){ return ooo_model_instr{0, input_instr{}}; }};

  std::vector<std::invoke_result_t<decltype(uut)>> generated_instrs{};
  std::generate_n(std::back_inserter(generated_instrs), 10, std::ref(uut));
  std::vector<uint64_t> ids{};
  std::transform(std::begin(generated_instrs), std::end(generated_instrs), std::back_inserter(ids), [](const auto& x){ return x.instr_id; });

  REQUIRE_THAT(ids, MonotonicallyIncreasingMatcher{});
}

TEST_CASE("Two tracereaders produce monotonically increasing instruction IDs") {
  champsim::tracereader uuta{[](){ return ooo_model_instr{0, input_instr{}}; }};
  champsim::tracereader uutb{[](){ return ooo_model_instr{0, input_instr{}}; }};

  std::vector<std::invoke_result_t<decltype(uuta)>> generated_instrs{};
  std::generate_n(std::back_inserter(generated_instrs), 10, std::ref(uuta));
  std::generate_n(std::back_inserter(generated_instrs), 10, std::ref(uutb));
  std::vector<uint64_t> ids{};
  std::transform(std::begin(generated_instrs), std::end(generated_instrs), std::back_inserter(ids), [](const auto& x){ return x.instr_id; });

  REQUIRE_THAT(ids, MonotonicallyIncreasingMatcher{});
}
