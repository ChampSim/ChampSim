#include "catch.hpp"
#include "util.h"

namespace {
  template <typename T>
  struct identity
  {
    auto operator()(const T &elem) const
    {
      return elem;
    }
  };
}

SCENARIO("An empty lru_table misses") {
  GIVEN("An empty lru_table") {
    champsim::lru_table<int, ::identity<int>, ::identity<int>> uut{1, 1};

    WHEN("We check for a hit") {
      auto result = uut.check_hit(0xdeadbeef);

      THEN("The result is a miss") {
        REQUIRE_FALSE(result.has_value());
      }
    }
  }
}

SCENARIO("A lru_table can hit") {
  GIVEN("A lru_table with one element") {
    constexpr int data  = 0xcafebabe;
    champsim::lru_table<int, ::identity<int>, ::identity<int>> uut{1, 1};
    uut.fill(data);

    WHEN("We check for a hit") {
      auto result = uut.check_hit(data);

      THEN("The result matches the filled value") {
        REQUIRE(result.has_value());
        REQUIRE(result.value() == data);
      }
    }
  }
}

SCENARIO("A lru_table can miss") {
  GIVEN("A lru_table with one element") {
    constexpr int data  = 0xcafebabe;
    champsim::lru_table<int, ::identity<int>, ::identity<int>> uut{1, 1};
    uut.fill(data);

    WHEN("We check for a hit") {
      auto result = uut.check_hit(data-1);

      THEN("The result is a miss") {
        REQUIRE_FALSE(result.has_value());
      }
    }
  }
}

SCENARIO("A lru_table replaces LRU") {
  GIVEN("A lru_table with two elements") {
    constexpr int data  = 0xcafebabe;
    champsim::lru_table<int, ::identity<int>, ::identity<int>> uut{1, 2};
    uut.fill(data);
    uut.fill(data+1);

    WHEN("We add a new element") {
      uut.fill(data+2);

      THEN("A check to the first-added element misses") {
        auto result = uut.check_hit(data);

        REQUIRE_FALSE(result.has_value());
      }

      AND_THEN("A check to the second-added element hits") {
        auto result = uut.check_hit(data+1);

        REQUIRE(result.has_value());
        REQUIRE(result.value() == data+1);
      }

      AND_THEN("A check to the new element hits") {
        auto result = uut.check_hit(data+2);

        REQUIRE(result.has_value());
        REQUIRE(result.value() == data+2);
      }
    }
  }
}

SCENARIO("A lru_table exhibits set-associative behavior") {
  GIVEN("A lru_table with two elements") {
    constexpr int data  = 0xcafebabe;
    champsim::lru_table<int, ::identity<int>, ::identity<int>> uut{2, 1};
    uut.fill(data);
    uut.fill(data+1);

    WHEN("We add a new element to the same set as the first element") {
      uut.fill(data+2);

      THEN("A check to the first-added element misses") {
        auto result = uut.check_hit(data);

        REQUIRE_FALSE(result.has_value());
      }

      AND_THEN("A check to the second-added element misses") {
        auto result = uut.check_hit(data+1);

        REQUIRE(result.has_value());
        REQUIRE(result.value() == data+1);
      }

      AND_THEN("A check to the new element hits") {
        auto result = uut.check_hit(data+2);

        REQUIRE(result.has_value());
        REQUIRE(result.value() == data+2);
      }
    }

    AND_WHEN("We add a new element to the same set as the second element") {
      uut.fill(data+3);

      THEN("A check to the first-added element hits") {
        auto result = uut.check_hit(data);

        REQUIRE(result.has_value());
        REQUIRE(result.value() == data);
      }

      AND_THEN("A check to the second-added element misses") {
        auto result = uut.check_hit(data+1);

        REQUIRE_FALSE(result.has_value());
      }

      AND_THEN("A check to the new element hits") {
        auto result = uut.check_hit(data+3);

        REQUIRE(result.has_value());
        REQUIRE(result.value() == data+3);
      }
    }
  }
}

SCENARIO("A lru_table misses after invalidation") {
  GIVEN("A lru_table with one element") {
    constexpr int data  = 0xcafebabe;
    champsim::lru_table<int, ::identity<int>, ::identity<int>> uut{1, 1};
    uut.fill(data);

    WHEN("We invalidate the block") {
      uut.invalidate(data);

      THEN("A subsequent check results in a miss") {
        auto result = uut.check_hit(data);
        REQUIRE_FALSE(result.has_value());
      }
    }
  }
}

SCENARIO("A lru_table returns the evicted block on invalidation") {
  GIVEN("A lru_table with one element") {
    constexpr int data  = 0xcafebabe;
    champsim::lru_table<int, ::identity<int>, ::identity<int>> uut{1, 1};
    uut.fill(data);

    WHEN("We invalidate the block") {
      auto result = uut.invalidate(data);

      THEN("The returned value is the original block") {
        REQUIRE(result.has_value());
        REQUIRE(result.value() == data);
      }
    }
  }
}

