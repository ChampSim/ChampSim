#include <catch.hpp>

#include "../../../branch/hashed_perceptron/folded_shift_register.h"

using global_history = folded_shift_register<champsim::data::bits{12}>;

TEST_CASE("The global history can have zero length") {
  global_history ghist{champsim::data::bits{0}};

  CHECK(ghist.value() == 0);
  ghist.push_back(true);
  CHECK(ghist.value() == 0);
}

TEST_CASE("The global history starts at zero") {
  global_history ghist{champsim::data::bits{170}};

  REQUIRE(ghist.value() == 0);
}

TEST_CASE("The global history accepts pushes") {
  global_history ghist{champsim::data::bits{256}};

  ghist.push_back(true);
  ghist.push_back(false);
  REQUIRE(ghist.value() == 2);

  BENCHMARK("Pushing into a folded_shift_register") {
    ghist.push_back(true);
  };

  BENCHMARK("Finding the value of a folded_shift_register") {
    return ghist.value();
  };
}

TEST_CASE("The global history loses history past its length") {
  global_history ghist{champsim::data::bits{1}};

  ghist.push_back(true);
  ghist.push_back(false);
  REQUIRE(ghist.value() == 0);
}

TEST_CASE("The global history fold size is at least 12") {
  global_history ghist{champsim::data::bits{170}};

  auto takens = GENERATE(range(0u,11u));

  for (std::size_t i = 0; i < takens; ++i) {
    ghist.push_back(true);
  }
  auto evaluated = (1ull << takens) - 1;

  REQUIRE(ghist.value() == evaluated);
}

TEST_CASE("The global history folds zeros onto ones") {
  global_history ghist{champsim::data::bits{8*24}};

  auto loops_of_24 = GENERATE(1u, 3u, 5u, 7u);

  for (std::size_t l = 0; l < loops_of_24; ++l) {
    for (std::size_t i = 0; i < 6; ++i) {
      ghist.push_back(false);
      ghist.push_back(true);
    } // 0x555
    for (std::size_t i = 0; i < 6; ++i) {
      ghist.push_back(true);
      ghist.push_back(false);
    } // 0xaaa
  }
  auto evaluated = 0xfffull;

  INFO("Loops of 24: " << loops_of_24);
  REQUIRE(ghist.value() == evaluated);
}

TEST_CASE("The global history folds ones onto zeros") {
  global_history ghist{champsim::data::bits{8*24}};

  auto loops_of_24 = GENERATE(1u, 3u, 5u, 7u);

  for (std::size_t l = 0; l < loops_of_24; ++l) {
    for (std::size_t i = 0; i < 6; ++i) {
      ghist.push_back(true);
      ghist.push_back(false);
    } // 0xaaa
    for (std::size_t i = 0; i < 6; ++i) {
      ghist.push_back(false);
      ghist.push_back(true);
    } // 0x555
  }
  auto evaluated = 0xfffull;

  INFO("Loops of 24: " << loops_of_24);
  REQUIRE(ghist.value() == evaluated);
}

TEST_CASE("The global history folds ones onto itself") {
  global_history ghist{champsim::data::bits{170}};

  auto loops_of_24 = GENERATE(range(0u, 8u));

  for (std::size_t i = 0; i < 12*loops_of_24; ++i) {
    ghist.push_back(true);
    ghist.push_back(false);
  } // 0xaaa
  auto evaluated = 0ull;

  INFO("Loops of 24: " << loops_of_24);
  REQUIRE(ghist.value() == evaluated);
}

TEST_CASE("The global history folds once") {
  global_history ghist{champsim::data::bits{24}};

  auto takens = GENERATE(range(12u,23u));

  for (std::size_t i = 0; i < takens; ++i) {
    ghist.push_back(true);
  }
  auto evaluated = 0xfffull ^ ((1ull << (takens-12)) - 1);

  REQUIRE(ghist.value() == evaluated);
}

TEST_CASE("The global history fold works when the fold size fits cleanly into the value size") {
  constexpr static std::size_t size = 8*64;
  folded_shift_register<champsim::data::bits{64}> ghist{champsim::data::bits{size}};

  auto takens = GENERATE(range(size,size+8));

  for (std::size_t i = 0; i < takens; ++i) {
    ghist.push_back(true);
  }
  auto evaluated = 0ull;

  REQUIRE(ghist.value() == evaluated);
}

TEST_CASE("The global history folds twice") {
  global_history ghist{champsim::data::bits{36}};

  auto takens = GENERATE(range(24u,35u));

  for (std::size_t i = 0; i < takens; ++i) {
    ghist.push_back(true);
  }
  auto evaluated = (1ull << (takens-24)) - 1;

  REQUIRE(ghist.value() == evaluated);
}
