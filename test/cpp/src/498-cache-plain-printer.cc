#include <catch.hpp>

#include "stats_printer.h"
#include "cache_stats.h"

TEST_CASE("An empty cache stat block prints all zeros") {
  cache_stats given{};
  given.name = "test_cache";

  std::vector<std::string> expected{
    "test_cache TOTAL        ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache PREFETCH REQUESTED:          0 ISSUED:          0 USEFUL:          0 USELESS:          0",
    "test_cache AVERAGE MISS LATENCY: - cycles"
  };

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Hits increment the hit and access counts") {
  auto num_hits = 255;
  auto [line_index, hit_type, expected_line] = GENERATE(as<std::tuple<std::size_t, access_type, std::string>>{},
      std::tuple{1, access_type::LOAD, "test_cache LOAD         ACCESS:        255 HIT:        255 MISS:          0"},
      std::tuple{2, access_type::RFO, "test_cache RFO          ACCESS:        255 HIT:        255 MISS:          0"},
      std::tuple{3, access_type::PREFETCH, "test_cache PREFETCH     ACCESS:        255 HIT:        255 MISS:          0"},
      std::tuple{4, access_type::WRITE, "test_cache WRITE        ACCESS:        255 HIT:        255 MISS:          0"},
      std::tuple{5, access_type::TRANSLATION, "test_cache TRANSLATION  ACCESS:        255 HIT:        255 MISS:          0"}
  );

  cache_stats given{};
  given.name = "test_cache";
  given.hits.set({hit_type, 0}, num_hits);

  std::vector<std::string> expected{
    "test_cache TOTAL        ACCESS:        255 HIT:        255 MISS:          0",
    "test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache PREFETCH REQUESTED:          0 ISSUED:          0 USEFUL:          0 USELESS:          0",
    "test_cache AVERAGE MISS LATENCY: - cycles"
  };
  expected.at(line_index) = expected_line;

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Misses increment the miss and access counts") {
  auto num_misses = 255;
  auto [line_index, miss_type, expected_line] = GENERATE(as<std::tuple<std::size_t, access_type, std::string>>{},
      std::tuple{1, access_type::LOAD, "test_cache LOAD         ACCESS:        255 HIT:          0 MISS:        255"},
      std::tuple{2, access_type::RFO, "test_cache RFO          ACCESS:        255 HIT:          0 MISS:        255"},
      std::tuple{3, access_type::PREFETCH, "test_cache PREFETCH     ACCESS:        255 HIT:          0 MISS:        255"},
      std::tuple{4, access_type::WRITE, "test_cache WRITE        ACCESS:        255 HIT:          0 MISS:        255"},
      std::tuple{5, access_type::TRANSLATION, "test_cache TRANSLATION  ACCESS:        255 HIT:          0 MISS:        255"}
  );

  cache_stats given{};
  given.name = "test_cache";
  given.misses.set({miss_type, 0}, num_misses);

  std::vector<std::string> expected{
    "test_cache TOTAL        ACCESS:        255 HIT:          0 MISS:        255",
    "test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache PREFETCH REQUESTED:          0 ISSUED:          0 USEFUL:          0 USELESS:          0",
    "test_cache AVERAGE MISS LATENCY: 0 cycles"
  };
  expected.at(line_index) = expected_line;

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Misses increment the AMAT") {
  auto num_misses = 255;
  auto miss_latency = GENERATE(1,2,6);
  auto [line_index, miss_type, expected_line] = GENERATE(as<std::tuple<std::size_t, access_type, std::string>>{},
      std::tuple{1, access_type::LOAD, "test_cache LOAD         ACCESS:        255 HIT:          0 MISS:        255"},
      std::tuple{2, access_type::RFO, "test_cache RFO          ACCESS:        255 HIT:          0 MISS:        255"},
      std::tuple{3, access_type::PREFETCH, "test_cache PREFETCH     ACCESS:        255 HIT:          0 MISS:        255"},
      std::tuple{4, access_type::WRITE, "test_cache WRITE        ACCESS:        255 HIT:          0 MISS:        255"},
      std::tuple{5, access_type::TRANSLATION, "test_cache TRANSLATION  ACCESS:        255 HIT:          0 MISS:        255"}
  );

  cache_stats given{};
  given.name = "test_cache";
  given.misses.set({miss_type, 0}, num_misses);
  given.total_miss_latency_cycles = miss_latency*num_misses;

  std::vector<std::string> expected{
    "test_cache TOTAL        ACCESS:        255 HIT:          0 MISS:        255",
    "test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache PREFETCH REQUESTED:          0 ISSUED:          0 USEFUL:          0 USELESS:          0",
  };
  expected.push_back("test_cache AVERAGE MISS LATENCY: " + std::to_string(miss_latency) + " cycles");
  expected.at(line_index) = expected_line;

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Prefetch requests increase the count") {
  cache_stats given{};
  given.name = "test_cache";
  given.pf_requested = 1;

  std::vector<std::string> expected{
    "test_cache TOTAL        ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache PREFETCH REQUESTED:          1 ISSUED:          0 USEFUL:          0 USELESS:          0",
    "test_cache AVERAGE MISS LATENCY: - cycles"
  };

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Prefetch issues increase the count") {
  cache_stats given{};
  given.name = "test_cache";
  given.pf_issued = 1;

  std::vector<std::string> expected{
    "test_cache TOTAL        ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache PREFETCH REQUESTED:          0 ISSUED:          1 USEFUL:          0 USELESS:          0",
    "test_cache AVERAGE MISS LATENCY: - cycles"
  };

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Prefetch useful increases the count") {
  cache_stats given{};
  given.name = "test_cache";
  given.pf_useful = 1;

  std::vector<std::string> expected{
    "test_cache TOTAL        ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache PREFETCH REQUESTED:          0 ISSUED:          0 USEFUL:          1 USELESS:          0",
    "test_cache AVERAGE MISS LATENCY: - cycles"
  };

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Prefetch useless increases the count") {
  cache_stats given{};
  given.name = "test_cache";
  given.pf_useless = 1;

  std::vector<std::string> expected{
    "test_cache TOTAL        ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0",
    "test_cache PREFETCH REQUESTED:          0 ISSUED:          0 USEFUL:          0 USELESS:          1",
    "test_cache AVERAGE MISS LATENCY: - cycles"
  };

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}
