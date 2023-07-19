#include <catch.hpp>

#include "cache.h"

#include <array>

#include "channel.h"

TEST_CASE("The sets factor uses the number of upper levels to determine the cache's default number of sets") {
  auto num_uls = GENERATE(1u,2u,4u,6u);
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
  auto sets_factor = GENERATE(1u,2u,4u,6u);
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

  buildA.sets(6);

  CACHE uut{buildA};

  REQUIRE(uut.NUM_SET == 6);
}

TEST_CASE("The MSHR factor uses the number of upper levels to determine the cache's default number of MSHRs") {
  auto num_uls = GENERATE(1u,2u,4u,6u);
  auto mshr_factor = 2u;
  champsim::cache_builder buildA{};
  std::vector<champsim::channel> channels{num_uls};
  std::vector<champsim::channel*> channel_pointers{};
  for (auto& elem: channels)
    channel_pointers.push_back(&elem);
  buildA.upper_levels(std::move(channel_pointers));
  buildA.mshr_factor((double)mshr_factor);

  CACHE uut{buildA};

  REQUIRE(uut.MSHR_SIZE == mshr_factor*num_uls);
}

TEST_CASE("The MSHR factor can control the cache's default number of MSHRs") {
  auto num_uls = 2u;
  auto mshr_factor = GENERATE(1u,2u,4u,6u);
  champsim::cache_builder buildA{};
  std::vector<champsim::channel> channels{num_uls};
  std::vector<champsim::channel*> channel_pointers{};
  for (auto& elem: channels)
    channel_pointers.push_back(&elem);
  buildA.upper_levels(std::move(channel_pointers));
  buildA.mshr_factor((double)mshr_factor);

  CACHE uut{buildA};

  REQUIRE(uut.MSHR_SIZE == mshr_factor*num_uls);
}

TEST_CASE("Specifying the MSHR size overrides the MSHR factor") {
  auto num_uls = 2u;
  auto mshr_factor = 2u;
  champsim::cache_builder buildA{};
  std::vector<champsim::channel> channels{num_uls};
  std::vector<champsim::channel*> channel_pointers{};
  for (auto& elem: channels)
    channel_pointers.push_back(&elem);
  buildA.upper_levels(std::move(channel_pointers));
  buildA.mshr_factor((double)mshr_factor);

  buildA.mshr_size(6);

  CACHE uut{buildA};

  REQUIRE(uut.MSHR_SIZE == 6);
}

TEST_CASE("The bandwidth factor uses the number of upper levels to determine the cache's default tag and fill bandwidth") {
  auto num_uls = GENERATE(1u,2u,4u,6u);
  auto bandwidth_factor = 2u;
  champsim::cache_builder buildA{};
  std::vector<champsim::channel> channels{num_uls};
  std::vector<champsim::channel*> channel_pointers{};
  for (auto& elem: channels)
    channel_pointers.push_back(&elem);
  buildA.upper_levels(std::move(channel_pointers));
  buildA.bandwidth_factor((double)bandwidth_factor);

  CACHE uut{buildA};

  CHECK(uut.MAX_TAG == bandwidth_factor*num_uls);
  CHECK(uut.MAX_FILL == bandwidth_factor*num_uls);
}

TEST_CASE("The bandwidth factor can control the cache's default tag bandwidth") {
  auto num_uls = 2u;
  auto bandwidth_factor = GENERATE(1u,2u,4u,6u);
  champsim::cache_builder buildA{};
  std::vector<champsim::channel> channels{num_uls};
  std::vector<champsim::channel*> channel_pointers{};
  for (auto& elem: channels)
    channel_pointers.push_back(&elem);
  buildA.upper_levels(std::move(channel_pointers));
  buildA.bandwidth_factor((double)bandwidth_factor);

  CACHE uut{buildA};

  CHECK(uut.MAX_TAG == bandwidth_factor*num_uls);
  CHECK(uut.MAX_FILL == bandwidth_factor*num_uls);
}

TEST_CASE("Specifying the tag bandwidth overrides the bandwidth factor") {
  auto num_uls = 2u;
  auto bandwidth_factor = 2u;
  champsim::cache_builder buildA{};
  std::vector<champsim::channel> channels{num_uls};
  std::vector<champsim::channel*> channel_pointers{};
  for (auto& elem: channels)
    channel_pointers.push_back(&elem);
  buildA.upper_levels(std::move(channel_pointers));
  buildA.bandwidth_factor((double)bandwidth_factor);

  buildA.tag_bandwidth(6);
  buildA.fill_bandwidth(7);

  CACHE uut{buildA};

  CHECK(uut.MAX_TAG == 6);
  CHECK(uut.MAX_FILL == 7);
}

TEST_CASE("If the hit latency is not specified, it is derived from the total latency") {
  auto latency = GENERATE(2u,4u,6u,10u);
  champsim::cache_builder buildA{};
  buildA.latency(latency);

  CACHE uut{buildA};

  REQUIRE((uut.HIT_LATENCY + uut.FILL_LATENCY) == latency*uut.clock_period );
}

TEST_CASE("The hit latency overrides the cache's total latency") {
  champsim::cache_builder buildA{};
  buildA.hit_latency(2);
  buildA.fill_latency(2);
  buildA.latency(10);

  CACHE uut{buildA};

  CHECK(uut.HIT_LATENCY == 2*uut.clock_period);
  CHECK(uut.FILL_LATENCY == 2*uut.clock_period);
}
