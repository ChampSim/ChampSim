#include <catch.hpp>

#include "cache.h"

#include <array>

#include "channel.h"

TEST_CASE("The sets can be specified by the logarithm") {
  auto log2_sets = GENERATE(6u, 10u, 20u);
  champsim::cache_builder buildA{};
  buildA.log2_sets(log2_sets);

  CACHE uut{buildA};

  REQUIRE(uut.NUM_SET == (1ull << log2_sets));
}

TEST_CASE("The number of sets is a power of two") {
  auto sets = 6u;
  champsim::cache_builder buildA{};
  buildA.sets(sets);

  CACHE uut{buildA};

  REQUIRE(uut.NUM_SET == 8);
}

TEST_CASE("The ways can be specified by the logarithm") {
  auto log2_ways = GENERATE(6u, 10u, 20u);
  champsim::cache_builder buildA{};
  buildA.log2_ways(log2_ways);

  CACHE uut{buildA};

  REQUIRE(uut.NUM_WAY == (1ull << log2_ways));
}

TEST_CASE("The sets factor uses the number of upper levels to determine the cache's default number of sets") {
  auto num_uls = GENERATE(1u,2u,4u,8u);
  auto sets_factor = 2u;
  champsim::cache_builder buildA{};
  std::vector<champsim::channel> channels{num_uls};
  std::vector<champsim::channel*> channel_pointers{};
  for (auto& elem: channels)
    channel_pointers.push_back(&elem);
  buildA.upper_levels(std::move(channel_pointers));
  buildA.sets_factor((double)sets_factor);

  CACHE uut{buildA};

  REQUIRE(uut.NUM_SET == sets_factor*num_uls);
}

TEST_CASE("The sets factor can control the cache's default number of sets") {
  auto num_uls = 2u;
  auto sets_factor = GENERATE(1u,2u,4u,8u);
  champsim::cache_builder buildA{};
  std::vector<champsim::channel> channels{num_uls};
  std::vector<champsim::channel*> channel_pointers{};
  for (auto& elem: channels)
    channel_pointers.push_back(&elem);
  buildA.upper_levels(std::move(channel_pointers));
  buildA.sets_factor((double)sets_factor);

  CACHE uut{buildA};

  REQUIRE(uut.NUM_SET == sets_factor*num_uls);
}

TEST_CASE("Specifying the sets overrides the cache's sets factor") {
  auto num_uls = 2u;
  auto sets_factor = 2u;
  champsim::cache_builder buildA{};
  std::vector<champsim::channel> channels{num_uls};
  std::vector<champsim::channel*> channel_pointers{};
  for (auto& elem: channels)
    channel_pointers.push_back(&elem);
  buildA.upper_levels(std::move(channel_pointers));
  buildA.sets_factor((double)sets_factor);

  buildA.sets(8);

  CACHE uut{buildA};

  REQUIRE(uut.NUM_SET == 8);
}

TEST_CASE("Specifying the size infers the cache's number of sets") {
  CACHE uut{champsim::cache_builder{}.size(16*1024).ways(16).offset_bits(6)};
  REQUIRE(uut.NUM_SET == 16);
}

TEST_CASE("Specifying the logarithm of the size infers the cache's number of sets") {
  CACHE uut{champsim::cache_builder{}.log2_size(14).ways(16).offset_bits(6)};
  REQUIRE(uut.NUM_SET == 16);
}

TEST_CASE("Specifying the size infers the cache's number of ways") {
  CACHE uut{champsim::cache_builder{}.size(16*1024).sets(16).offset_bits(6)};
  REQUIRE(uut.NUM_WAY == 16);
}

TEST_CASE("Specifying the logarithm of the size infers the cache's number of ways") {
  CACHE uut{champsim::cache_builder{}.log2_size(14).sets(16).offset_bits(6)};
  REQUIRE(uut.NUM_WAY == 16);
}

TEST_CASE("The number of MSHRs scales with the number of sets") {
  auto num_sets = GENERATE(8u, 32u, 256u, 1024u, 65536u);
  auto fill_bandwidth = 2u;
  auto fill_latency = 2u;
  auto expected_mshrs = num_sets*fill_bandwidth*fill_latency/16;
  champsim::cache_builder buildA{};
  buildA.sets(num_sets);
  buildA.fill_bandwidth(champsim::bandwidth::maximum_type{fill_bandwidth});
  buildA.fill_latency(fill_latency);

  CACHE uut{buildA};

  REQUIRE(uut.MSHR_SIZE == expected_mshrs);
}

TEST_CASE("The number of MSHRs scales with the fill bandwidth") {
  auto num_sets = 256u;
  auto fill_bandwidth = GENERATE(1u, 2u, 3u, 8u);
  auto fill_latency = 2u;
  auto expected_mshrs = num_sets*fill_bandwidth*fill_latency/16;
  champsim::cache_builder buildA{};
  buildA.sets(num_sets);
  buildA.fill_bandwidth(champsim::bandwidth::maximum_type{fill_bandwidth});
  buildA.fill_latency(fill_latency);

  CACHE uut{buildA};

  REQUIRE(uut.MSHR_SIZE == expected_mshrs);
}

TEST_CASE("The number of MSHRs scales with the fill latency") {
  auto num_sets = 256u;
  auto fill_bandwidth = 2u;
  auto fill_latency = GENERATE(1u, 2u, 3u, 8u);
  auto expected_mshrs = num_sets*fill_bandwidth*fill_latency/16;
  champsim::cache_builder buildA{};
  buildA.sets(num_sets);
  buildA.fill_bandwidth(champsim::bandwidth::maximum_type{fill_bandwidth});
  buildA.fill_latency(fill_latency);

  CACHE uut{buildA};

  REQUIRE(uut.MSHR_SIZE == expected_mshrs);
}

TEST_CASE("Specifying the MSHR size overrides the inferred MSHR size") {
  auto fill_bandwidth = 2u;
  auto fill_latency = 2u;
  auto num_mshrs = 6u;
  champsim::cache_builder buildA{};
  buildA.fill_bandwidth(champsim::bandwidth::maximum_type{fill_bandwidth});
  buildA.fill_latency(fill_latency);
  buildA.mshr_size(num_mshrs);

  CACHE uut{buildA};

  REQUIRE(uut.MSHR_SIZE == num_mshrs);
}

TEST_CASE("If no bandwidth is specified, it is derived from the number of sets") {
  auto [num_sets, expected_bandwidth] = GENERATE(
      std::pair{16, 1},
      std::pair{64, 1},
      std::pair{1024, 2},
      std::pair{4*1024, 8},
      std::pair{32*1024, 64}
  );
  champsim::cache_builder buildA{};
  buildA.sets(num_sets);

  CACHE uut{buildA};

  CHECK(uut.MAX_TAG == champsim::bandwidth::maximum_type{expected_bandwidth});
  CHECK(uut.MAX_FILL == champsim::bandwidth::maximum_type{expected_bandwidth});
}

TEST_CASE("Specifying the tag bandwidth overrides inference from the number of sets") {
  auto num_sets = 1024;
  champsim::cache_builder buildA{};
  buildA.sets(num_sets);
  buildA.tag_bandwidth(champsim::bandwidth::maximum_type{6});
  buildA.fill_bandwidth(champsim::bandwidth::maximum_type{7});

  CACHE uut{buildA};

  CHECK(uut.MAX_TAG == champsim::bandwidth::maximum_type{6});
  CHECK(uut.MAX_FILL == champsim::bandwidth::maximum_type{7});
}

TEST_CASE("If no latency is specified, it is derived from the size") {
  auto [size, hit_latency, fill_latency] = GENERATE(
      std::tuple{32*1024, 3ull, 3ull},
      std::tuple{512*1024, 7ull, 7ull},
      std::tuple{8*1024*1024, 11ull, 11ull}
  );
  champsim::cache_builder buildA{};
  buildA.size(size);
  buildA.ways(8);

  CACHE uut{buildA};

  CHECK(uut.HIT_LATENCY == hit_latency);
  CHECK(uut.FILL_LATENCY == fill_latency);
}

TEST_CASE("If the hit and fill latency are not specified, they are derived from the total latency") {
  auto latency = GENERATE(2u,4u,6u,10u);
  champsim::cache_builder buildA{};
  buildA.latency(latency);

  CACHE uut{buildA};

  CHECK(uut.HIT_LATENCY == latency/2);
  CHECK(uut.FILL_LATENCY == latency/2);
}

TEST_CASE("If the hit latency is not specified, it is derived from the fill latency and total latency") {
  auto latency = GENERATE(4u,6u,10u);
  auto fill_latency = GENERATE(1u,2u,3u);
  champsim::cache_builder buildA{};
  buildA.latency(latency);
  buildA.fill_latency(fill_latency);

  CACHE uut{buildA};

  CHECK(uut.HIT_LATENCY == latency-fill_latency);
  CHECK(uut.FILL_LATENCY == fill_latency);
}

TEST_CASE("The hit latency overrides the cache's total latency") {
  champsim::cache_builder buildA{};
  buildA.hit_latency(2);
  buildA.fill_latency(3);
  buildA.latency(10);

  CACHE uut{buildA};

  CHECK(uut.HIT_LATENCY == 2);
  CHECK(uut.FILL_LATENCY == 3);
}
