#include <catch.hpp>

#include "champsim.h"
#include "util/units.h"

TEST_CASE("A data size is default-constructible, with default value zero") {
  champsim::data::bytes test{};
  REQUIRE(test.count() == 0);
}

TEST_CASE("A data size is constructible with a value") {
  champsim::data::bytes test{2};
  REQUIRE(test.count() == 2);
}

TEST_CASE("A data size is copy-constructible") {
  champsim::data::bytes seed{2016};
  champsim::data::bytes test{seed};
  REQUIRE(seed.count() == test.count());
}

TEST_CASE("A data size is move-constructible") {
  champsim::data::bytes seed{2016};
  champsim::data::bytes test{std::move(seed)};
  REQUIRE(test.count() == 2016);
}

TEST_CASE("A data size is copy-assignable") {
  champsim::data::bytes seed{2016};
  champsim::data::bytes test{};
  test = seed;
  REQUIRE(seed.count() == test.count());
}

TEST_CASE("A data size is move-assignable") {
  champsim::data::bytes seed{2016};
  champsim::data::bytes test{};
  test = std::move(seed);
  REQUIRE(test.count() == 2016);
}

TEST_CASE("A data size is converting constructible") {
  champsim::data::kibibytes seed{2};
  champsim::data::bytes test{seed};
  REQUIRE(seed.count()*1024 == test.count());
}

TEST_CASE("Data size literals work") {
  using champsim::data::data_literals::operator""_B;
  using champsim::data::data_literals::operator""_kiB;
  using champsim::data::data_literals::operator""_MiB;
  using champsim::data::data_literals::operator""_GiB;
  using champsim::data::data_literals::operator""_TiB;

  CHECK(1_kiB == 1024_B);
  CHECK(2_kiB == 2048_B);

  CHECK(1_MiB == 1024_kiB);
  CHECK(1_MiB == 1048576_B);
  CHECK(2_MiB == 2048_kiB);

  CHECK(1_GiB == 1024_MiB);
  CHECK(1_GiB == 1048576_kiB);
  CHECK(1_GiB == 1073741824_B);
  CHECK(2_GiB == 2048_MiB);

  CHECK(1_TiB == 1024_GiB);
  CHECK(1_TiB == 1048576_MiB);
  CHECK(1_TiB == 1073741824_kiB);
  CHECK(1_TiB == 1099511627776_B);
  CHECK(2_TiB == 2048_GiB);
}

TEST_CASE("Data sizes can be added in-place") {
  using champsim::data::data_literals::operator""_kiB;
  auto test{1_kiB};
  test += 2_kiB;
  REQUIRE(test == 3_kiB);
}

TEST_CASE("Data sizes can be added") {
  using champsim::data::data_literals::operator""_kiB;
  REQUIRE(1_kiB + 2_kiB == 3_kiB);
}

TEST_CASE("Data sizes can be subtracted in-place") {
  using champsim::data::data_literals::operator""_kiB;
  auto test{3_kiB};
  test -= 2_kiB;
  REQUIRE(test == 1_kiB);
}

TEST_CASE("Data sizes can be subtracted") {
  using champsim::data::data_literals::operator""_B;
  using champsim::data::data_literals::operator""_kiB;
  REQUIRE(1_kiB - 1_B == 1023_B);
}

TEST_CASE("Data sizes can be scaled in-place") {
  using champsim::data::data_literals::operator""_kiB;
  auto test{1_kiB};
  test *= 2;
  REQUIRE(test == 2_kiB);
}

TEST_CASE("Data sizes can be scaled") {
  using champsim::data::data_literals::operator""_kiB;
  REQUIRE(1_kiB * 2 == 2_kiB);
}

TEST_CASE("Data sizes can be divided in-place") {
  using champsim::data::data_literals::operator""_kiB;
  auto test{2_kiB};
  test /= 2;
  REQUIRE(test == 1_kiB);
}

TEST_CASE("Data sizes can be divided") {
  using champsim::data::data_literals::operator""_kiB;
  REQUIRE(2_kiB / 2 == 1_kiB);
}

TEST_CASE("Data sizes can be divided by sizes") {
  using champsim::data::data_literals::operator""_kiB;
  REQUIRE(2_kiB / 1_kiB == 2);
}

TEST_CASE("Data sizes are ordered") {
  using champsim::data::data_literals::operator""_MiB;
  CHECK(1_MiB == 1_MiB);
  CHECK_FALSE(1_MiB != 1_MiB);
  CHECK_FALSE(1_MiB < 1_MiB);
  CHECK_FALSE(1_MiB > 1_MiB);
  CHECK(1_MiB <= 1_MiB);
  CHECK(1_MiB >= 1_MiB);

  CHECK_FALSE(1_MiB == 2_MiB);
  CHECK(1_MiB != 2_MiB);
  CHECK(1_MiB < 2_MiB);
  CHECK_FALSE(1_MiB > 2_MiB);
  CHECK(1_MiB <= 2_MiB);
  CHECK_FALSE(1_MiB >= 2_MiB);

  CHECK_FALSE(2_MiB == 1_MiB);
  CHECK(2_MiB != 1_MiB);
  CHECK_FALSE(2_MiB < 1_MiB);
  CHECK(2_MiB > 1_MiB);
  CHECK_FALSE(2_MiB <= 1_MiB);
  CHECK(2_MiB >= 1_MiB);
}

TEST_CASE("A byte size prints something to libfmt") {
  using namespace champsim::data::data_literals;
  REQUIRE_THAT(fmt::format("{}", 5_B), Catch::Matchers::Matches("5 B"));
  REQUIRE_THAT(fmt::format("{}", 5_kiB), Catch::Matchers::Matches("5 kiB"));
  REQUIRE_THAT(fmt::format("{}", 5_MiB), Catch::Matchers::Matches("5 MiB"));
  REQUIRE_THAT(fmt::format("{}", 5_GiB), Catch::Matchers::Matches("5 GiB"));
  REQUIRE_THAT(fmt::format("{}", 5_TiB), Catch::Matchers::Matches("5 TiB"));
}

