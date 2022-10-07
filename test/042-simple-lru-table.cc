#include "catch.hpp"
#include "util.h"

SCENARIO("An empty simple_lru_table misses") {
  GIVEN("An empty simple_lru_table") {
    champsim::simple_lru_table<uint64_t, uint64_t> uut{1, 1, 0};

    WHEN("We check for a hit") {
      auto result = uut.check_hit(0xdeadbeef);

      THEN("The result is a miss") {
        REQUIRE_FALSE(result.has_value());
      }
    }
  }
}

SCENARIO("A simple_lru_table can hit") {
  GIVEN("A simple_lru_table with one element") {
    constexpr uint64_t index = 0xdeadbeef;
    constexpr uint64_t data  = 0xcafebabe;
    champsim::simple_lru_table<uint64_t, uint64_t> uut{1, 1, 0};
    uut.fill_cache(index, data);

    WHEN("We check for a hit") {
      auto result = uut.check_hit(index);

      THEN("The result matches the filled value") {
        REQUIRE(result.value_or(0x12345678) == data);
      }
    }
  }
}

SCENARIO("A simple_lru_table can miss") {
  GIVEN("A simple_lru_table with one element") {
    constexpr uint64_t index = 0xdeadbeef;
    constexpr uint64_t data  = 0xcafebabe;
    champsim::simple_lru_table<uint64_t, uint64_t> uut{1, 1, 0};
    uut.fill_cache(index, data);

    WHEN("We check for a hit") {
      auto result = uut.check_hit(index-1);

      THEN("The result is a miss") {
        REQUIRE_FALSE(result.has_value());
      }
    }
  }
}

SCENARIO("A simple_lru_table can hit with respect to the shamt") {
  GIVEN("A simple_lru_table with one element") {
    constexpr uint64_t index = 0xdeadbeef;
    constexpr uint64_t data  = 0xcafebabe;
    constexpr std::size_t shamt = 8;
    champsim::simple_lru_table<uint64_t, uint64_t> uut{1, 1, shamt};
    uut.fill_cache(index, data);

    WHEN("We check for a hit inside the shamt") {
      auto new_index = 0xdeadbe88;
      auto result = uut.check_hit(new_index);

      THEN("The result matches the filled value") {
        REQUIRE(result.value_or(0x12345678) == data);
      }
    }

    AND_WHEN("We check for a hit outside the shamt") {
      auto new_index = 0xdeadb888;
      auto result = uut.check_hit(new_index);

      THEN("The result is a miss") {
        REQUIRE_FALSE(result.has_value());
      }
    }
  }
}

SCENARIO("A simple_lru_table replaces LRU") {
  GIVEN("A simple_lru_table with two elements") {
    constexpr uint64_t index = 0xdeadbeef;
    constexpr uint64_t data  = 0xcafebabe;
    champsim::simple_lru_table<uint64_t, uint64_t> uut{1, 2, 0};
    uut.fill_cache(index, data);
    uut.fill_cache(index+1, data+1);

    WHEN("We add a new element") {
      uut.fill_cache(index+2, data+2);

      THEN("A check to the first-added element misses") {
        auto result = uut.check_hit(index);

        REQUIRE_FALSE(result.has_value());
      }

      AND_THEN("A check to the second-added element hits") {
        auto result = uut.check_hit(index+1);

        REQUIRE(result.value_or(0x12345678) == data+1);
      }

      AND_THEN("A check to the new element hits") {
        auto result = uut.check_hit(index+2);

        REQUIRE(result.value_or(0x12345678) == data+2);
      }
    }
  }
}

SCENARIO("A simple_lru_table exhibits set-associative behavior") {
  GIVEN("A simple_lru_table with two elements") {
    constexpr uint64_t index = 0xdeadbeef;
    constexpr uint64_t data  = 0xcafebabe;
    champsim::simple_lru_table<uint64_t, uint64_t> uut{2, 1, 0};
    uut.fill_cache(index, data);
    uut.fill_cache(index+1, data+1);

    WHEN("We add a new element to the same set as the first element") {
      uut.fill_cache(index+2, data+2);

      THEN("A check to the first-added element misses") {
        auto result = uut.check_hit(index);

        REQUIRE_FALSE(result.has_value());
      }

      AND_THEN("A check to the second-added element misses") {
        auto result = uut.check_hit(index+1);

        REQUIRE(result.value_or(0x12345678) == data+1);
      }

      AND_THEN("A check to the new element hits") {
        auto result = uut.check_hit(index+2);

        REQUIRE(result.value_or(0x12345678) == data+2);
      }
    }

    AND_WHEN("We add a new element to the same set as the second element") {
      uut.fill_cache(index+3, data+3);

      THEN("A check to the first-added element hits") {
        auto result = uut.check_hit(index);

        REQUIRE(result.value_or(0x12345678) == data);
      }

      AND_THEN("A check to the second-added element misses") {
        auto result = uut.check_hit(index+1);

        REQUIRE_FALSE(result.has_value());
      }

      AND_THEN("A check to the new element hits") {
        auto result = uut.check_hit(index+3);

        REQUIRE(result.value_or(0x12345678) == data+3);
      }
    }
  }
}

SCENARIO("A simple_lru_table misses after invalidation") {
  GIVEN("A simple_lru_table with one element") {
    constexpr uint64_t index = 0xdeadbeef;
    constexpr uint64_t data  = 0xcafebabe;
    constexpr std::size_t shamt = 8;
    champsim::simple_lru_table<uint64_t, uint64_t> uut{1, 1, shamt};
    uut.fill_cache(index, data);

    WHEN("We invalidate the block") {
      uut.invalidate(index);

      THEN("A subsequent check results in a miss") {
        auto result = uut.check_hit(index);
        REQUIRE_FALSE(result.has_value());
      }
    }
  }
}

SCENARIO("A simple_lru_table returns the evicted block on invalidation") {
  GIVEN("A simple_lru_table with one element") {
    constexpr uint64_t index = 0xdeadbeef;
    constexpr uint64_t data  = 0xcafebabe;
    constexpr std::size_t shamt = 8;
    champsim::simple_lru_table<uint64_t, uint64_t> uut{1, 1, shamt};
    uut.fill_cache(index, data);

    WHEN("We invalidate the block") {
      auto result = uut.invalidate(index);

      THEN("The returned value is the original block") {
        REQUIRE(result.value_or(0x12345678) == data);
      }
    }
  }
}
