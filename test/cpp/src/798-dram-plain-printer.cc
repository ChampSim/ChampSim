#include <catch.hpp>

#include "stats_printer.h"
#include "dram_stats.h"

TEST_CASE("An empty DRAM stats prints zero")
{
  dram_stats given{};
  given.name = "test_channel";

  std::vector<std::string> expected{
    "test_channel RQ ROW_BUFFER_HIT:          0",
    "  ROW_BUFFER_MISS:          0",
    "  AVG DBUS CONGESTED CYCLE: -",
    "test_channel WQ ROW_BUFFER_HIT:          0",
    "  ROW_BUFFER_MISS:          0",
    "  FULL:          0",
    "test_channel REFRESHES ISSUED: -"
  };

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("The DRAM RQ row buffer hit counter increments the printed stats")
{
  dram_stats given{};
  given.name = "test_channel";
  given.RQ_ROW_BUFFER_HIT = 255;

  std::vector<std::string> expected{
    "test_channel RQ ROW_BUFFER_HIT:        255",
    "  ROW_BUFFER_MISS:          0",
    "  AVG DBUS CONGESTED CYCLE: -",
    "test_channel WQ ROW_BUFFER_HIT:          0",
    "  ROW_BUFFER_MISS:          0",
    "  FULL:          0",
    "test_channel REFRESHES ISSUED: -"
  };

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("The DRAM RQ row buffer miss counter increments the printed stats")
{
  dram_stats given{};
  given.name = "test_channel";
  given.RQ_ROW_BUFFER_MISS = 255;

  std::vector<std::string> expected{
    "test_channel RQ ROW_BUFFER_HIT:          0",
    "  ROW_BUFFER_MISS:        255",
    "  AVG DBUS CONGESTED CYCLE: -",
    "test_channel WQ ROW_BUFFER_HIT:          0",
    "  ROW_BUFFER_MISS:          0",
    "  FULL:          0",
    "test_channel REFRESHES ISSUED: -"
  };

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("The DRAM WQ row buffer hit counter increments the printed stats")
{
  dram_stats given{};
  given.name = "test_channel";
  given.WQ_ROW_BUFFER_HIT = 255;

  std::vector<std::string> expected{
    "test_channel RQ ROW_BUFFER_HIT:          0",
    "  ROW_BUFFER_MISS:          0",
    "  AVG DBUS CONGESTED CYCLE: -",
    "test_channel WQ ROW_BUFFER_HIT:        255",
    "  ROW_BUFFER_MISS:          0",
    "  FULL:          0",
    "test_channel REFRESHES ISSUED: -"
  };

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("The DRAM WQ row buffer miss counter increments the printed stats")
{
  dram_stats given{};
  given.name = "test_channel";
  given.WQ_ROW_BUFFER_MISS = 255;

  std::vector<std::string> expected{
    "test_channel RQ ROW_BUFFER_HIT:          0",
    "  ROW_BUFFER_MISS:          0",
    "  AVG DBUS CONGESTED CYCLE: -",
    "test_channel WQ ROW_BUFFER_HIT:          0",
    "  ROW_BUFFER_MISS:        255",
    "  FULL:          0",
    "test_channel REFRESHES ISSUED: -"
  };

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("The DRAM WQ full counter increments the printed stats")
{
  dram_stats given{};
  given.name = "test_channel";
  given.WQ_FULL = 255;

  std::vector<std::string> expected{
    "test_channel RQ ROW_BUFFER_HIT:          0",
    "  ROW_BUFFER_MISS:          0",
    "  AVG DBUS CONGESTED CYCLE: -",
    "test_channel WQ ROW_BUFFER_HIT:          0",
    "  ROW_BUFFER_MISS:          0",
    "  FULL:        255",
    "test_channel REFRESHES ISSUED: -"
  };

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("The DRAM dbus congestion counters increment the printed stats")
{
  dram_stats given{};
  given.name = "test_channel";
  given.dbus_cycle_congested = 100;
  given.dbus_count_congested = 100;

  std::vector<std::string> expected{
    "test_channel RQ ROW_BUFFER_HIT:          0",
    "  ROW_BUFFER_MISS:          0",
    "  AVG DBUS CONGESTED CYCLE: 1",
    "test_channel WQ ROW_BUFFER_HIT:          0",
    "  ROW_BUFFER_MISS:          0",
    "  FULL:          0",
    "test_channel REFRESHES ISSUED: -"
  };

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("The DRAM refresh counters increment the printed stats")
{
  dram_stats given{};
  given.name = "test_channel";
  given.refresh_cycles = 100;

  std::vector<std::string> expected{
    "test_channel RQ ROW_BUFFER_HIT:          0",
    "  ROW_BUFFER_MISS:          0",
    "  AVG DBUS CONGESTED CYCLE: -",
    "test_channel WQ ROW_BUFFER_HIT:          0",
    "  ROW_BUFFER_MISS:          0",
    "  FULL:          0",
    "test_channel REFRESHES ISSUED:        100"
  };

  REQUIRE_THAT(champsim::plain_printer::format(given), Catch::Matchers::RangeEquals(expected));
}
