#include <catch.hpp>

#include "config.h"

#include <vector>
#include <map>

TEST_CASE("int_or_prefixed_size() passes through integers") {
  CHECK(champsim::config::int_or_prefixed_size(1l) == 1l);
  CHECK(champsim::config::int_or_prefixed_size(10l) == 10l);
  CHECK(champsim::config::int_or_prefixed_size(100l) == 100l);
}

TEST_CASE("int_or_prefixed_size() parses strings") {
  CHECK(champsim::config::int_or_prefixed_size("1") == 1l);
  CHECK(champsim::config::int_or_prefixed_size("10") == 10l);
  CHECK(champsim::config::int_or_prefixed_size("100") == 100l);

  CHECK(champsim::config::int_or_prefixed_size("1B") == 1l);
  CHECK(champsim::config::int_or_prefixed_size("10B") == 10l);
  CHECK(champsim::config::int_or_prefixed_size("100B") == 100l);

  CHECK(champsim::config::int_or_prefixed_size("1k") == 1l*1024);
  CHECK(champsim::config::int_or_prefixed_size("10k") == 10l*1024);
  CHECK(champsim::config::int_or_prefixed_size("100k") == 100l*1024);

  CHECK(champsim::config::int_or_prefixed_size("1kB") == 1l*1024);
  CHECK(champsim::config::int_or_prefixed_size("10kB") == 10l*1024);
  CHECK(champsim::config::int_or_prefixed_size("100kB") == 100l*1024);

  CHECK(champsim::config::int_or_prefixed_size("1kiB") == 1l*1024);
  CHECK(champsim::config::int_or_prefixed_size("10kiB") == 10l*1024);
  CHECK(champsim::config::int_or_prefixed_size("100kiB") == 100l*1024);

  CHECK(champsim::config::int_or_prefixed_size("1M") == 1l*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("10M") == 10l*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("100M") == 100l*1024*1024);

  CHECK(champsim::config::int_or_prefixed_size("1MB") == 1l*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("10MB") == 10l*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("100MB") == 100l*1024*1024);

  CHECK(champsim::config::int_or_prefixed_size("1MiB") == 1l*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("10MiB") == 10l*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("100MiB") == 100l*1024*1024);

  CHECK(champsim::config::int_or_prefixed_size("1G") == 1l*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("10G") == 10l*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("100G") == 100l*1024*1024*1024);

  CHECK(champsim::config::int_or_prefixed_size("1GB") == 1l*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("10GB") == 10l*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("100GB") == 100l*1024*1024*1024);

  CHECK(champsim::config::int_or_prefixed_size("1GiB") == 1l*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("10GiB") == 10l*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("100GiB") == 100l*1024*1024*1024);

  CHECK(champsim::config::int_or_prefixed_size("1T") == 1l*1024*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("10T") == 10l*1024*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("100T") == 100l*1024*1024*1024*1024);

  CHECK(champsim::config::int_or_prefixed_size("1TB") == 1l*1024*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("10TB") == 10l*1024*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("100TB") == 100l*1024*1024*1024*1024);

  CHECK(champsim::config::int_or_prefixed_size("1TiB") == 1l*1024*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("10TiB") == 10l*1024*1024*1024*1024);
  CHECK(champsim::config::int_or_prefixed_size("100TiB") == 100l*1024*1024*1024*1024);
}

TEST_CASE("propogate() returns the second value if both keys are absent")
{
  std::map<std::string, long> lhs{{"test", 100}};
  std::map<std::string, long> rhs{{"test", 200}};
  auto result = champsim::config::propogate(lhs, rhs, "absent");
  REQUIRE_THAT(result, Catch::Matchers::RangeEquals(rhs));
}

TEST_CASE("propogate() returns the second value if both keys are present")
{
  std::map<std::string, long> lhs{{"test", 100}};
  std::map<std::string, long> rhs{{"test", 200}};
  auto result = champsim::config::propogate(lhs, rhs, "test");
  REQUIRE_THAT(result, Catch::Matchers::RangeEquals(rhs));
}

TEST_CASE("propogate() returns the second value if it has a key")
{
  std::map<std::string, long> lhs{};
  std::map<std::string, long> rhs{{"test", 200}};
  auto result = champsim::config::propogate(lhs, rhs, "test");
  REQUIRE_THAT(result, Catch::Matchers::RangeEquals(rhs));
}

TEST_CASE("propogate() returns the second value, modified, if it does not have a key and the first does")
{
  std::map<std::string, long> lhs{{"test", 100}};
  std::map<std::string, long> rhs{};
  std::map<std::string, long> expected{{"test", 100}};
  auto result = champsim::config::propogate(lhs, rhs, "test");
  REQUIRE_THAT(result, Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("propogate_down() passes through an empty sequence")
{
  std::vector<std::map<std::string, long>> given{};
  std::string given_key{"test"};

  std::vector<typename decltype(given)::value_type::mapped_type> expected{};

  auto result = champsim::config::propogate_down(given, given_key);
  std::vector<typename decltype(given)::value_type::mapped_type> evaluated{};
  std::transform(std::begin(result), std::end(result), std::back_inserter(evaluated), [given_key](auto x){ return x[given_key]; });

  REQUIRE_THAT(evaluated, Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("propogate_down() passes through a full sequence")
{
  std::string given_key{"test"};
  std::vector<std::map<std::string, long>> given{
    std::map<std::string, long>{{given_key, 1}},
    std::map<std::string, long>{{given_key, 2}},
    std::map<std::string, long>{{given_key, 3}},
    std::map<std::string, long>{{given_key, 4}},
    std::map<std::string, long>{{given_key, 5}},
  };

  std::vector<typename decltype(given)::value_type::mapped_type> expected{{1,2,3,4,5}};

  auto result = champsim::config::propogate_down(given, given_key);
  std::vector<typename decltype(given)::value_type::mapped_type> evaluated{};
  std::transform(std::begin(result), std::end(result), std::back_inserter(evaluated), [given_key](auto x){ return x[given_key]; });

  REQUIRE_THAT(evaluated, Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("propogate_down() fills gaps in an incomplete sequence")
{
  std::string given_key{"test"};
  std::vector<std::map<std::string, long>> given{
    std::map<std::string, long>{{given_key, 1}},
    std::map<std::string, long>{},
    std::map<std::string, long>{{given_key, 3}},
    std::map<std::string, long>{},
    std::map<std::string, long>{},
  };

  std::vector<typename decltype(given)::value_type::mapped_type> expected{{1,1,3,3,3}};

  auto result = champsim::config::propogate_down(given, given_key);
  std::vector<typename decltype(given)::value_type::mapped_type> evaluated{};
  std::transform(std::begin(result), std::end(result), std::back_inserter(evaluated), [given_key](auto x){ return x[given_key]; });

  REQUIRE_THAT(evaluated, Catch::Matchers::RangeEquals(expected));
}
