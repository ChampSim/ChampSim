#include <catch.hpp>
#include <map>
#include <random>
#include <type_traits>
#include <fmt/format.h>

#include "util/random.h"

using namespace fmt::literals;

/**
 * Test whether a uniform int distribution is generating values uniformly.
 *
 * \tparam URBG The type of random number generator to use
 * \param num_samples The number of samples to draw
 * \param num_buckets The number of buckets to distribute samples into
 * \param tolerance The maximum percent deviation from the expected number of
                    samples per bucket for this test to pass
 * \param seed The seed to use
 */
template <class URBG>
void test_if_uniform_int_distribution_is_uniform(std::size_t num_samples, std::size_t num_buckets, double tolerance, unsigned long seed = 0)
{
  using dist_type = int;

  // Expected number of samples per bucket
  const std::size_t expected_samples = num_samples / num_buckets;
  const std::size_t min_samples = static_cast<std::size_t>(static_cast<double>(expected_samples) * (1.0 - tolerance));
  const std::size_t max_samples = static_cast<std::size_t>(static_cast<double>(expected_samples) * (1.0 + tolerance));

  // Create the distribution and generator
  champsim::uniform_int_distribution<dist_type> uut{0, static_cast<dist_type>(num_buckets - 1)};
  URBG generator{seed};

  // Generate many samples and count their occurences
  std::map<dist_type, std::size_t> buckets{};
  for (std::size_t sample_i = 0; sample_i < num_samples; sample_i++) {
    buckets[uut(generator)]++;
  }

  // Check that the number of samples in each bucket is within the expected range
  for (auto& [bucket, count] : buckets) {
    if (count < min_samples || count > max_samples) {
      FAIL(fmt::format("Bucket {bucket} has {actual} samples, expected {expected} (tolerance: {min} to {max})",
                       "bucket"_a = bucket,             // Value tested
                       "actual"_a = count,              // Actual count for this value
                       "expected"_a = expected_samples, // Expected count for this value
                       "min"_a = min_samples,           // Minimum acceptable count for this value the test will accept
                       "max"_a = max_samples            // Maximum acceptable count for this value
                       ));
    }
  }

  SUCCEED();
}

/**
 * Test that we can construct a uniform_int_distribution with a valid type.
 *
 * \tparam IntType The type to test
 */
template <typename IntType>
void test_uniform_int_distribution_type()
{
  // Check if constructible
  STATIC_REQUIRE(std::is_constructible<champsim::uniform_int_distribution<IntType>>::value);

  // Check if result_type is correct
  STATIC_REQUIRE(std::is_same<typename champsim::uniform_int_distribution<IntType>::result_type, IntType>::value);
}

/**
 * Test that we can't construct a uniform_int_distribution with an invalid type.
 *
 * \tparam NonIntType The type to test
 */
template <typename NonIntType>
void test_uniform_int_distribution_type_fails()
{
  // Check if constructible
  STATIC_REQUIRE_FALSE(std::is_constructible<champsim::uniform_int_distribution<NonIntType>>::value);
}

/**
 * Test that uniform_int_distribution default parameters are set and retrieved correctly.
 *
 * \tparam IntType The type to test
 */
template <typename IntType>
void test_uniform_int_distribution_default_parameters()
{
  using param_type = typename champsim::uniform_int_distribution<IntType>::param_type;

  constexpr IntType a_expected{0};
  constexpr IntType b_expected{std::numeric_limits<IntType>::max()};

  // uut_1: Default parameters
  champsim::uniform_int_distribution<IntType> uut_1{};

  // Get parameters
  REQUIRE(uut_1.a() == a_expected);
  REQUIRE(uut_1.b() == b_expected);
  REQUIRE(uut_1.param() == param_type{a_expected, b_expected});
  REQUIRE(uut_1.param().a() == a_expected);
  REQUIRE(uut_1.param().b() == b_expected);

  // uut_2: One parameter
  constexpr IntType a{30};
  champsim::uniform_int_distribution<IntType> uut_2{a};

  // Get parameters
  REQUIRE(uut_2.a() == a);
  REQUIRE(uut_2.b() == b_expected);
  REQUIRE(uut_2.param() == param_type{a, b_expected});
  REQUIRE(uut_2.param().a() == a);
  REQUIRE(uut_2.param().b() == b_expected);
}

/**
 * Test that the uniform_int_distribution's parameters are set and retrieved correctly.
 *
 * \tparam IntType The type to test
 */
template <typename IntType>
void test_uniform_int_distribution_custom_parameters()
{
  using param_type = typename champsim::uniform_int_distribution<IntType>::param_type;

  constexpr IntType a{0};
  constexpr IntType b{10};

  constexpr IntType a_new{5};
  constexpr IntType b_new{15};

  // uut_1: Fully parameterized
  champsim::uniform_int_distribution<IntType> uut_1{a, b};

  // Get parameters
  REQUIRE(uut_1.a() == a);
  REQUIRE(uut_1.b() == b);
  REQUIRE(uut_1.param() == param_type{a, b});
  REQUIRE(uut_1.param().a() == a);
  REQUIRE(uut_1.param().b() == b);

  // Set parameters
  uut_1.param(param_type{a_new, b_new});
  REQUIRE(uut_1.a() == a_new);
  REQUIRE(uut_1.b() == b_new);
  REQUIRE(uut_1.param() == param_type{a_new, b_new});
  REQUIRE(uut_1.param().a() == a_new);
  REQUIRE(uut_1.param().b() == b_new);

  // uut_2: Fully parameterized with param_type
  champsim::uniform_int_distribution<IntType> uut_2{param_type{a, b}};
  REQUIRE(uut_2.a() == a);
  REQUIRE(uut_2.b() == b);
  REQUIRE(uut_2.param() == param_type{a, b});
  REQUIRE(uut_2.param().a() == a);
  REQUIRE(uut_2.param().b() == b);
}

TEST_CASE("The uniform int distribution is constructed with the right type")
{
  // Make sure integral types are constructible
  SECTION("char") { test_uniform_int_distribution_type<char>(); }
  SECTION("unsigned char") { test_uniform_int_distribution_type<unsigned char>(); }
  SECTION("short") { test_uniform_int_distribution_type<short>(); }
  SECTION("unsigned short") { test_uniform_int_distribution_type<unsigned short>(); }
  SECTION("int") { test_uniform_int_distribution_type<int>(); }
  SECTION("unsigned int") { test_uniform_int_distribution_type<unsigned int>(); }
  SECTION("long") { test_uniform_int_distribution_type<long>(); }
  SECTION("unsigned long") { test_uniform_int_distribution_type<unsigned long>(); }
  SECTION("long long") { test_uniform_int_distribution_type<long long>(); }
  SECTION("unsigned long long") { test_uniform_int_distribution_type<unsigned long long>(); }

  // Make sure non-integral types are not constructible
  SECTION("float") { test_uniform_int_distribution_type_fails<float>(); }
  SECTION("double") { test_uniform_int_distribution_type_fails<double>(); }
  SECTION("long double") { test_uniform_int_distribution_type_fails<long double>(); }
  SECTION("std::string") { test_uniform_int_distribution_type_fails<std::string>(); }
  SECTION("std::pair<int, int>") { test_uniform_int_distribution_type_fails<std::pair<int, int>>(); }
  SECTION("std::vector<int>") { test_uniform_int_distribution_type_fails<std::vector<int>>(); }
}

TEST_CASE("The uniform int distribution is constructed with the right parameters")
{
  SECTION("Default parameters")
  {
    SECTION("int") { test_uniform_int_distribution_default_parameters<int>(); }
    SECTION("unsigned int") { test_uniform_int_distribution_default_parameters<unsigned int>(); }
    SECTION("long") { test_uniform_int_distribution_default_parameters<long>(); }
    SECTION("unsigned long") { test_uniform_int_distribution_default_parameters<unsigned long>(); }
  }

  SECTION("Custom parameters")
  {
    SECTION("int") { test_uniform_int_distribution_custom_parameters<int>(); }
    SECTION("unsigned int") { test_uniform_int_distribution_custom_parameters<unsigned int>(); }
    SECTION("long") { test_uniform_int_distribution_custom_parameters<long>(); }
    SECTION("unsigned long") { test_uniform_int_distribution_custom_parameters<unsigned long>(); }
  }
}

TEST_CASE("The uniform int distribution is approximately uniform") { test_if_uniform_int_distribution_is_uniform<std::mt19937_64>(1'000'000, 3, 0.01, 0); }

TEST_CASE("The uniform int distribution ostream operator works")
{
  using dist_type = int;

  // Should pass
  champsim::uniform_int_distribution<dist_type> uut{0, 10};
  std::ostringstream oss;
  oss << uut;

  REQUIRE(oss.str() == "0 10");
}

TEST_CASE("The uniform int distribution istream operator works")
{
  using dist_type = int;

  // Should pass
  champsim::uniform_int_distribution<dist_type> uut_1{};
  std::istringstream iss_1{"5 15"};
  iss_1 >> uut_1;

  REQUIRE(uut_1.a() == 5);
  REQUIRE(uut_1.b() == 15);

  // Should fail (since min > max)
  champsim::uniform_int_distribution<dist_type> uut_2{};
  std::istringstream iss_2{"20 10"};
  iss_2 >> uut_2;

  REQUIRE(iss_2.fail());
}