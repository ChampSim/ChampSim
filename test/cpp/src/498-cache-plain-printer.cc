#include <catch.hpp>

#include "stats_printer.h"
#include "cache_stats.h"

TEST_CASE("An empty cache stat block prints nothing") {
  cache_stats given{};
  given.name = "test_cache";

  std::vector<std::string> expected{};

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Hits increment the hit and access counts") {
  auto num_hits = 255;
  auto [line_index, hit_type, expected_line] = GENERATE(as<std::tuple<std::size_t, access_type, std::string>>{},
      std::tuple{1, access_type::LOAD, "cpu0->test_cache LOAD         ACCESS:        255 HIT:        255 MISS:          0 MSHR_MERGE:          0"},
      std::tuple{2, access_type::RFO, "cpu0->test_cache RFO          ACCESS:        255 HIT:        255 MISS:          0 MSHR_MERGE:          0"},
      std::tuple{3, access_type::PREFETCH, "cpu0->test_cache PREFETCH     ACCESS:        255 HIT:        255 MISS:          0 MSHR_MERGE:          0"},
      std::tuple{4, access_type::WRITE, "cpu0->test_cache WRITE        ACCESS:        255 HIT:        255 MISS:          0 MSHR_MERGE:          0"},
      std::tuple{5, access_type::TRANSLATION, "cpu0->test_cache TRANSLATION  ACCESS:        255 HIT:        255 MISS:          0 MSHR_MERGE:          0"}
  );

  cache_stats given{};
  given.name = "test_cache";
  given.hits.set({hit_type, 0}, num_hits);

  std::vector<std::string> expected{
    "cpu0->test_cache TOTAL        ACCESS:        255 HIT:        255 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache PREFETCH REQUESTED:          0 ISSUED:          0 USEFUL:          0 USELESS:          0",
    "cpu0->test_cache AVERAGE MISS LATENCY: - cycles"
  };
  expected.at(line_index) = expected_line;

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Misses increment the miss and access counts") {
  auto num_misses = 255;
  auto [line_index, miss_type, expected_line] = GENERATE(as<std::tuple<std::size_t, access_type, std::string>>{},
      std::tuple{1, access_type::LOAD, "cpu0->test_cache LOAD         ACCESS:        255 HIT:          0 MISS:        255 MSHR_MERGE:          0"},
      std::tuple{2, access_type::RFO, "cpu0->test_cache RFO          ACCESS:        255 HIT:          0 MISS:        255 MSHR_MERGE:          0"},
      std::tuple{3, access_type::PREFETCH, "cpu0->test_cache PREFETCH     ACCESS:        255 HIT:          0 MISS:        255 MSHR_MERGE:          0"},
      std::tuple{4, access_type::WRITE, "cpu0->test_cache WRITE        ACCESS:        255 HIT:          0 MISS:        255 MSHR_MERGE:          0"},
      std::tuple{5, access_type::TRANSLATION, "cpu0->test_cache TRANSLATION  ACCESS:        255 HIT:          0 MISS:        255 MSHR_MERGE:          0"}
  );

  cache_stats given{};
  given.name = "test_cache";
  given.misses.set({miss_type, 0}, num_misses);

  std::vector<std::string> expected{
    "cpu0->test_cache TOTAL        ACCESS:        255 HIT:          0 MISS:        255 MSHR_MERGE:          0",
    "cpu0->test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache PREFETCH REQUESTED:          0 ISSUED:          0 USEFUL:          0 USELESS:          0",
    "cpu0->test_cache AVERAGE MISS LATENCY: - cycles"
  };
  expected.at(line_index) = expected_line;

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Returning MSHRs increment the AMAT") {
  auto num_mshr_returned = 128;
  auto num_mshr_merged = 127;
  auto mshr_return_latency = GENERATE(1,2,6);
  auto [line_index, miss_type, expected_line] = GENERATE(as<std::tuple<std::size_t, access_type, std::string>>{},
      std::tuple{1, access_type::LOAD, "cpu0->test_cache LOAD         ACCESS:        255 HIT:          0 MISS:        255 MSHR_MERGE:        127"},
      std::tuple{2, access_type::RFO, "cpu0->test_cache RFO          ACCESS:        255 HIT:          0 MISS:        255 MSHR_MERGE:        127"},
      //std::tuple{3, access_type::PREFETCH, "test_cache PREFETCH     ACCESS:        255 HIT:          0 MISS:        255 MSHR_MERGE:        127"},
      std::tuple{4, access_type::WRITE, "cpu0->test_cache WRITE        ACCESS:        255 HIT:          0 MISS:        255 MSHR_MERGE:        127"},
      std::tuple{5, access_type::TRANSLATION, "cpu0->test_cache TRANSLATION  ACCESS:        255 HIT:          0 MISS:        255 MSHR_MERGE:        127"}
  );

  cache_stats given{};
  given.name = "test_cache";
  given.mshr_return.set({miss_type, 0}, num_mshr_returned);
  given.mshr_merge.set({miss_type, 0}, num_mshr_merged);
  given.misses.set({miss_type,0}, num_mshr_merged + num_mshr_returned);
  given.total_miss_latency_cycles = mshr_return_latency*num_mshr_returned;

  std::vector<std::string> expected{
    "cpu0->test_cache TOTAL        ACCESS:        255 HIT:          0 MISS:        255 MSHR_MERGE:        127",
    "cpu0->test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache PREFETCH REQUESTED:          0 ISSUED:          0 USEFUL:          0 USELESS:          0",
  };
  expected.push_back("cpu0->test_cache AVERAGE MISS LATENCY: " + std::to_string(mshr_return_latency) + " cycles");
  expected.at(line_index) = expected_line;

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Prefetch requests increase the count") {
  cache_stats given{};
  given.name = "test_cache";
  given.pf_requested = 1;
  given.mshr_return.set({access_type::PREFETCH,0},1);

  std::vector<std::string> expected{
    "cpu0->test_cache TOTAL        ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache PREFETCH REQUESTED:          1 ISSUED:          0 USEFUL:          0 USELESS:          0",
    "cpu0->test_cache AVERAGE MISS LATENCY: - cycles"
  };

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Prefetch issues increase the count") {
  cache_stats given{};
  given.name = "test_cache";
  given.pf_issued = 1;
  given.mshr_return.set({access_type::PREFETCH,0},1);

  std::vector<std::string> expected{
    "cpu0->test_cache TOTAL        ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache PREFETCH REQUESTED:          0 ISSUED:          1 USEFUL:          0 USELESS:          0",
    "cpu0->test_cache AVERAGE MISS LATENCY: - cycles"
  };

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Prefetch useful increases the count") {
  cache_stats given{};
  given.name = "test_cache";
  given.pf_useful = 1;
  given.mshr_return.set({access_type::PREFETCH,0},1);

  std::vector<std::string> expected{
    "cpu0->test_cache TOTAL        ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache PREFETCH REQUESTED:          0 ISSUED:          0 USEFUL:          1 USELESS:          0",
    "cpu0->test_cache AVERAGE MISS LATENCY: - cycles"
  };

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Prefetch useless increases the count") {
  cache_stats given{};
  given.name = "test_cache";
  given.pf_useless = 1;
  given.mshr_return.set({access_type::PREFETCH,0},1);

  std::vector<std::string> expected{
    "cpu0->test_cache TOTAL        ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache PREFETCH REQUESTED:          0 ISSUED:          0 USEFUL:          0 USELESS:          1",
    "cpu0->test_cache AVERAGE MISS LATENCY: - cycles"
  };

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("Multicore stats are tracked separately") {
  cache_stats given{};
  given.name = "test_cache";
  auto constexpr cpu0_total_access = 7;
  auto constexpr cpu1_total_access = 11;

  auto [line_index_cpu0, hit_type_cpu0, expected_line_cpu0] = GENERATE(as<std::tuple<std::size_t, access_type, std::string>>{},
      std::tuple{1, access_type::LOAD, "cpu0->test_cache LOAD         ACCESS:          7 HIT:          7 MISS:          0 MSHR_MERGE:          0"},
      std::tuple{2, access_type::RFO, "cpu0->test_cache RFO          ACCESS:          7 HIT:          7 MISS:          0 MSHR_MERGE:          0"},
      std::tuple{3, access_type::PREFETCH, "cpu0->test_cache PREFETCH     ACCESS:          7 HIT:          7 MISS:          0 MSHR_MERGE:          0"},
      std::tuple{4, access_type::WRITE, "cpu0->test_cache WRITE        ACCESS:          7 HIT:          7 MISS:          0 MSHR_MERGE:          0"},
      std::tuple{5, access_type::TRANSLATION, "cpu0->test_cache TRANSLATION  ACCESS:          7 HIT:          7 MISS:          0 MSHR_MERGE:          0"}
  );
  auto [line_index_cpu1, hit_type_cpu1, expected_line_cpu1] = GENERATE(as<std::tuple<std::size_t, access_type, std::string>>{},
      std::tuple{9, access_type::LOAD, "cpu1->test_cache LOAD         ACCESS:         11 HIT:         11 MISS:          0 MSHR_MERGE:          0"},
      std::tuple{10,access_type::RFO, "cpu1->test_cache RFO          ACCESS:         11 HIT:         11 MISS:          0 MSHR_MERGE:          0"},
      std::tuple{11,access_type::PREFETCH, "cpu1->test_cache PREFETCH     ACCESS:         11 HIT:         11 MISS:          0 MSHR_MERGE:          0"},
      std::tuple{12,access_type::WRITE, "cpu1->test_cache WRITE        ACCESS:         11 HIT:         11 MISS:          0 MSHR_MERGE:          0"},
      std::tuple{13,access_type::TRANSLATION, "cpu1->test_cache TRANSLATION  ACCESS:         11 HIT:         11 MISS:          0 MSHR_MERGE:          0"}
  );
  given.hits.set({hit_type_cpu0, 0}, cpu0_total_access);
  given.hits.set({hit_type_cpu1, 1}, cpu1_total_access);

  
  std::vector<std::string> expected{
    "cpu0->test_cache TOTAL        ACCESS:          7 HIT:          7 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu0->test_cache PREFETCH REQUESTED:          0 ISSUED:          0 USEFUL:          0 USELESS:          0",
    "cpu0->test_cache AVERAGE MISS LATENCY: - cycles",
    "cpu1->test_cache TOTAL        ACCESS:         11 HIT:         11 MISS:          0 MSHR_MERGE:          0",
    "cpu1->test_cache LOAD         ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu1->test_cache RFO          ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu1->test_cache PREFETCH     ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu1->test_cache WRITE        ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu1->test_cache TRANSLATION  ACCESS:          0 HIT:          0 MISS:          0 MSHR_MERGE:          0",
    "cpu1->test_cache PREFETCH REQUESTED:          0 ISSUED:          0 USEFUL:          0 USELESS:          0",
    "cpu1->test_cache AVERAGE MISS LATENCY: - cycles"
  };
  expected.at(line_index_cpu0) = expected_line_cpu0;
  expected.at(line_index_cpu1) = expected_line_cpu1;

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}