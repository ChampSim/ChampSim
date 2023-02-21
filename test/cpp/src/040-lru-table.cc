#include <catch.hpp>
#include "util/lru_table.h"

#include "champsim.h"
#include "address.h"

#include <type_traits>

namespace {
  template <typename T>
  struct strong_type
  {
    T value;
  };

  struct strong_type_getter
  {
    template <typename T>
    auto operator()(const strong_type<T> &elem) const
    {
      return elem.value;
    }
  };

  struct type_with_getters
  {
    unsigned int value;

    auto index() const
    {
      return value;
    }

    auto tag() const
    {
      return value;
    }
  };
}

TEMPLATE_TEST_CASE("An lru_table is copiable and moveable", "",
    (champsim::lru_table<::strong_type<unsigned int>, ::strong_type_getter, ::strong_type_getter>), champsim::lru_table<::type_with_getters>) {
  STATIC_REQUIRE(std::is_copy_constructible_v<TestType>);
  STATIC_REQUIRE(std::is_move_constructible_v<TestType>);
  STATIC_REQUIRE(std::is_copy_assignable_v<TestType>);
  STATIC_REQUIRE(std::is_move_assignable_v<TestType>);
}

TEMPLATE_TEST_CASE("An empty lru_table misses", "",
    (champsim::lru_table<::strong_type<unsigned int>, ::strong_type_getter, ::strong_type_getter>), champsim::lru_table<::type_with_getters>) {
  GIVEN("An empty lru_table") {
    TestType uut{1, 1};

    WHEN("We check for a hit") {
      auto result = uut.check_hit({2016});

      THEN("The result is a miss") {
        REQUIRE_FALSE(result.has_value());
      }
    }
  }
}

TEMPLATE_TEST_CASE("A lru_table can hit", "",
    (champsim::lru_table<::strong_type<unsigned int>, ::strong_type_getter, ::strong_type_getter>), champsim::lru_table<::type_with_getters>) {
  GIVEN("A lru_table with one element") {
    constexpr unsigned int data  = 0xcafebabe;
    TestType uut{1, 1};
    uut.fill({data});

    WHEN("We check for a hit") {
      auto result = uut.check_hit({data});

      THEN("The result matches the filled value") {
        REQUIRE(result.has_value());
        REQUIRE(result.value().value == data);
      }
    }
  }
}

TEMPLATE_TEST_CASE("A lru_table can miss", "",
    (champsim::lru_table<::strong_type<unsigned int>, ::strong_type_getter, ::strong_type_getter>), champsim::lru_table<::type_with_getters>) {
  GIVEN("A lru_table with one element") {
    constexpr unsigned int data  = 0xcafebabe;
    TestType uut{1, 1};
    uut.fill({data});

    WHEN("We check for a hit") {
      auto result = uut.check_hit({data-1});

      THEN("The result is a miss") {
        REQUIRE_FALSE(result.has_value());
      }
    }
  }
}

TEST_CASE("A lru_table of addresses can hit") {
  GIVEN("A lru_table with one element") {
    constexpr unsigned int data  = 0xcafebabe;
    champsim::lru_table<::strong_type<champsim::address>, ::strong_type_getter, ::strong_type_getter> uut{1, 1};
    uut.fill({champsim::address{data}});

    WHEN("We check for a hit") {
      auto result = uut.check_hit({champsim::address{data}});

      THEN("The result matches the filled value") {
        REQUIRE(result.has_value());
        REQUIRE(result.value().value == champsim::address{data});
      }
    }
  }
}

TEST_CASE("A lru_table of addresses can miss") {
  GIVEN("A lru_table with one element") {
    constexpr unsigned int data  = 0xcafebabe;
    champsim::lru_table<::strong_type<champsim::address>, ::strong_type_getter, ::strong_type_getter> uut{1, 1};
    uut.fill({champsim::address{data}});

    WHEN("We check for a hit") {
      auto result = uut.check_hit({champsim::address{data-1}});

      THEN("The result is a miss") {
        REQUIRE_FALSE(result.has_value());
      }
    }
  }
}

TEMPLATE_TEST_CASE("A lru_table replaces LRU", "",
    (champsim::lru_table<::strong_type<unsigned int>, ::strong_type_getter, ::strong_type_getter>), champsim::lru_table<::type_with_getters>) {
  GIVEN("A lru_table with two elements") {
    constexpr unsigned int data  = 0xcafebabe;
    TestType uut{1, 2};
    uut.fill({data});
    uut.fill({data+1});

    WHEN("We add a new element") {
      uut.fill({data+2});

      THEN("A check to the first-added element misses") {
        auto result = uut.check_hit({data});

        REQUIRE_FALSE(result.has_value());
      }

      THEN("A check to the second-added element hits") {
        auto result = uut.check_hit({data+1});

        REQUIRE(result.has_value());
        REQUIRE(result.value().value == data+1);
      }

      THEN("A check to the new element hits") {
        auto result = uut.check_hit({data+2});

        REQUIRE(result.has_value());
        REQUIRE(result.value().value == data+2);
      }
    }
  }
}

TEMPLATE_TEST_CASE("A lru_table exhibits set-associative behavior", "",
    (champsim::lru_table<::strong_type<unsigned int>, ::strong_type_getter, ::strong_type_getter>), champsim::lru_table<::type_with_getters>) {
  GIVEN("A lru_table with two elements") {
    constexpr unsigned int data  = 0xcafebabe;
    TestType uut{2, 1};
    uut.fill({data});
    uut.fill({data+1});

    WHEN("We add a new element to the same set as the first element") {
      uut.fill({data+2});

      THEN("A check to the first-added element misses") {
        auto result = uut.check_hit({data});

        REQUIRE_FALSE(result.has_value());
      }

      THEN("A check to the second-added element hits") {
        auto result = uut.check_hit({data+1});

        REQUIRE(result.has_value());
        REQUIRE(result.value().value == data+1);
      }

      THEN("A check to the new element hits") {
        auto result = uut.check_hit({data+2});

        REQUIRE(result.has_value());
        REQUIRE(result.value().value == data+2);
      }

      AND_WHEN("We add a new element to the same set as the second element") {
        uut.fill({data+3});

        THEN("A check to the first-added element misses") {
          auto result = uut.check_hit({data});

          REQUIRE_FALSE(result.has_value());
        }

        THEN("A check to the second-added element misses") {
          auto result = uut.check_hit({data+1});

          REQUIRE_FALSE(result.has_value());
        }

        THEN("A check to the third-added element hits") {
          auto result = uut.check_hit({data+2});

          REQUIRE(result.has_value());
          REQUIRE(result.value().value == data+2);
        }

        THEN("A check to the new element hits") {
          auto result = uut.check_hit({data+3});

          REQUIRE(result.has_value());
          REQUIRE(result.value().value == data+3);
        }
      }
    }
  }
}

TEMPLATE_TEST_CASE("A lru_table misses after invalidation", "",
    (champsim::lru_table<::strong_type<unsigned int>, ::strong_type_getter, ::strong_type_getter>), champsim::lru_table<::type_with_getters>) {
  GIVEN("A lru_table with one element") {
    constexpr unsigned int data  = 0xcafebabe;
    TestType uut{1, 1};
    uut.fill({data});

    WHEN("We invalidate the block") {
      uut.invalidate({data});

      THEN("A subsequent check results in a miss") {
        auto result = uut.check_hit({data});
        REQUIRE_FALSE(result.has_value());
      }
    }
  }
}

TEMPLATE_TEST_CASE("A lru_table returns the evicted block on invalidation", "",
    (champsim::lru_table<::strong_type<unsigned int>, ::strong_type_getter, ::strong_type_getter>), champsim::lru_table<::type_with_getters>) {
  GIVEN("A lru_table with one element") {
    constexpr unsigned int data  = 0xcafebabe;
    TestType uut{1, 1};
    uut.fill({data});

    WHEN("We invalidate the block") {
      auto result = uut.invalidate({data});

      THEN("The returned value is the original block") {
        REQUIRE(result.has_value());
        REQUIRE(result.value().value == data);
      }
    }
  }
}

