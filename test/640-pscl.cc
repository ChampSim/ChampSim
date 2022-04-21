#include "catch.hpp"
#include "mocks.hpp"
#include "ptw.h"

SCENARIO("An empty PSCL misses") {
  GIVEN("An empty PSCL") {
    PagingStructureCache uut{1, 1, 1, 0};

    WHEN("We check for a hit") {
      auto result = uut.check_hit(0xdeadbeef);

      THEN("The result is a miss") {
        REQUIRE_FALSE(result.has_value());
      }
    }
  }
}

SCENARIO("A PSCL can hit") {
  GIVEN("A PSCL with one element") {
    constexpr uint64_t index = 0xdeadbeef;
    constexpr uint64_t data  = 0xcafebabe;
    PagingStructureCache uut{1, 1, 1, 0};
    uut.fill_cache(data, index);

    WHEN("We check for a hit") {
      auto result = uut.check_hit(index);

      THEN("The result matches the filled value") {
        REQUIRE(result.has_value());
        REQUIRE(result.value() == data);
      }
    }
  }
}

SCENARIO("A PSCL can miss") {
  GIVEN("A PSCL with one element") {
    constexpr uint64_t index = 0xdeadbeef;
    constexpr uint64_t data  = 0xcafebabe;
    PagingStructureCache uut{1, 1, 1, 0};
    uut.fill_cache(data, index);

    WHEN("We check for a hit") {
      auto result = uut.check_hit(index-1);

      THEN("The result is a miss") {
        REQUIRE_FALSE(result.has_value());
      }
    }
  }
}

SCENARIO("A PSCL can hit with respect to the shamt") {
  GIVEN("A PSCL with one element") {
    constexpr uint64_t index = 0xdeadbeef;
    constexpr uint64_t data  = 0xcafebabe;
    constexpr std::size_t shamt = 8;
    PagingStructureCache uut{1, 1, 1, shamt};
    uut.fill_cache(data, index);

    WHEN("We check for a hit inside the shamt") {
      auto new_index = 0xdeadbe88;
      auto result = uut.check_hit(new_index);

      THEN("The result matches the filled value") {
        REQUIRE(result.has_value());
        REQUIRE(result.value() == data);
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

SCENARIO("A PSCL replaces LRU") {
  GIVEN("A PSCL with two elements") {
    constexpr uint64_t index = 0xdeadbeef;
    constexpr uint64_t data  = 0xcafebabe;
    PagingStructureCache uut{1, 1, 2, 0};
    uut.fill_cache(data, index);
    uut.fill_cache(data+1, index+1);

    WHEN("We add a new element") {
      uut.fill_cache(data+2, index+2);

      THEN("A check to the first-added element misses") {
        auto result = uut.check_hit(index);

        REQUIRE_FALSE(result.has_value());
      }

      AND_THEN("A check to the second-added element hits") {
        auto result = uut.check_hit(index+1);

        REQUIRE(result.has_value());
        REQUIRE(result.value() == data+1);
      }

      AND_THEN("A check to the new element hits") {
        auto result = uut.check_hit(index+2);

        REQUIRE(result.has_value());
        REQUIRE(result.value() == data+2);
      }
    }
  }
}

SCENARIO("A PSCL exhibits set-associative behavior") {
  GIVEN("A PSCL with two elements") {
    constexpr uint64_t index = 0xdeadbeef;
    constexpr uint64_t data  = 0xcafebabe;
    PagingStructureCache uut{1, 2, 1, 0};
    uut.fill_cache(data, index);
    uut.fill_cache(data+1, index+1);

    WHEN("We add a new element to the same set as the first element") {
      uut.fill_cache(data+2, index+2);

      THEN("A check to the first-added element misses") {
        auto result = uut.check_hit(index);

        REQUIRE_FALSE(result.has_value());
      }

      AND_THEN("A check to the second-added element misses") {
        auto result = uut.check_hit(index+1);

        REQUIRE(result.has_value());
        REQUIRE(result.value() == data+1);
      }

      AND_THEN("A check to the new element hits") {
        auto result = uut.check_hit(index+2);

        REQUIRE(result.has_value());
        REQUIRE(result.value() == data+2);
      }
    }

    AND_WHEN("We add a new element to the same set as the second element") {
      uut.fill_cache(data+3, index+3);

      THEN("A check to the first-added element hits") {
        auto result = uut.check_hit(index);

        REQUIRE(result.has_value());
        REQUIRE(result.value() == data);
      }

      AND_THEN("A check to the second-added element misses") {
        auto result = uut.check_hit(index+1);

        REQUIRE_FALSE(result.has_value());
      }

      AND_THEN("A check to the new element hits") {
        auto result = uut.check_hit(index+3);

        REQUIRE(result.has_value());
        REQUIRE(result.value() == data+3);
      }
    }
  }
}
