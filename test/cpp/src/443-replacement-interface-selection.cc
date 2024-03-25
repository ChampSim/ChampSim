#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"
#include "modules.h"

#include <map>
#include <vector>
namespace
{
  std::map<CACHE*, int> victim_interface_discerner;
  std::map<CACHE*, int> update_interface_discerner;
  std::map<CACHE*, int> fill_override_interface_discerner;

  struct dual_interface : champsim::modules::replacement
  {
    using replacement::replacement;

    long find_victim(uint32_t, uint64_t, long, const CACHE::BLOCK*, uint64_t, uint64_t, uint32_t)
    {
      ::victim_interface_discerner[intern_] = 1;
      return 0;
    }

    long find_victim(uint32_t, uint64_t, long, const CACHE::BLOCK*, champsim::address, champsim::address, uint32_t)
    {
      ::victim_interface_discerner[intern_] = 2;
      return 0;
    }

    long find_victim(uint32_t, uint64_t, long, const CACHE::BLOCK*, champsim::address, champsim::address, access_type)
    {
      ::victim_interface_discerner[intern_] = 3;
      return 0;
    }

    void update_replacement_state(uint32_t, long, long, uint64_t, uint64_t, uint64_t, uint32_t, uint8_t)
    {
      ::update_interface_discerner[intern_] = 1;
    }

    void update_replacement_state(uint32_t, long, long, champsim::address, champsim::address, champsim::address, uint32_t, bool)
    {
      ::update_interface_discerner[intern_] = 2;
    }

    void update_replacement_state(uint32_t, long, long, champsim::address, champsim::address, champsim::address, access_type, bool)
    {
      ::update_interface_discerner[intern_] = 3;
    }
  };

  struct fill_selection : champsim::modules::replacement
  {
    using replacement::replacement;

    long find_victim(uint32_t, uint64_t, long, const CACHE::BLOCK*, champsim::address, champsim::address, access_type)
    {
      return 0;
    }

    void update_replacement_state(uint32_t, long, long, champsim::address, champsim::address, access_type, bool)
    {
      ::update_interface_discerner[intern_] = 1;
    }

    void replacement_cache_fill(uint32_t, long, long, champsim::address, champsim::address, champsim::address, access_type)
    {
      ::fill_override_interface_discerner[intern_] = 1;
    }
  };
}

SCENARIO("The simulator selects the address-based victim finder in replacement policies") {
  using namespace std::literals;
  GIVEN("A single cache") {
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("443a-uut")
      .sets(1)
      .ways(1)
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .offset_bits(champsim::data::bits{})
      .replacement<::dual_interface, lru>()
    };

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A packet is issued") {
      ::victim_interface_discerner.insert_or_assign(&uut, 0);

      decltype(mock_ul)::request_type seed;
      seed.address = champsim::address{0xdeadbeef};
      seed.is_translated = true;
      seed.cpu = 0;
      auto seed_result = mock_ul.issue(seed);

      THEN("The issue is received") {
        REQUIRE(seed_result);
      }

      // Run the uut for a bunch of cycles to fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      AND_WHEN("The packet is returned") {
        // Run the uut for a bunch of cycles to fill the cache
        for (auto i = 0; i < 100; ++i)
          for (auto elem : elements)
            elem->_operate();

        AND_WHEN("A packet with a different address is sent") {
          decltype(mock_ul)::request_type test;
          test.address = champsim::address{0xcafebabe};
          test.is_translated = true;
          test.cpu = 0;
          auto test_result = mock_ul.issue(test);

          THEN("The issue is received") {
            REQUIRE(test_result);
          }

          // Run the uut for a bunch of cycles to fill the cache
          for (auto i = 0; i < 100; ++i)
            for (auto elem : elements)
              elem->_operate();

          THEN("The replacement policy is called with address interface") {
            REQUIRE(::victim_interface_discerner[&uut] == 3);
          }
        }
      }
    }
  }
}

SCENARIO("The simulator selects the address-based update function in replacement policies") {
  using namespace std::literals;
  auto [type, str] = GENERATE(table<access_type, std::string_view>({std::pair{access_type::LOAD, "load"sv}, std::pair{access_type::RFO, "RFO"sv}, std::pair{access_type::PREFETCH, "prefetch"sv}, std::pair{access_type::WRITE, "write"sv}, std::pair{access_type::TRANSLATION, "translation"sv}}));
  GIVEN("A cache with one element") {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l2c}
      .name("443b-uut-"+std::string{str})
      .sets(1)
      .ways(1)
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .prefetch_activate(type)
      .offset_bits(champsim::data::bits{})
      .replacement<::dual_interface, lru>()
    };

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    decltype(mock_ul)::request_type test;
    test.address = champsim::address{0xdeadbeef};
    test.address = champsim::address{0xdeadbeef};
    test.cpu = 0;
    test.type = type;
    auto test_result = mock_ul.issue(test);

    THEN("The issue is received") {
      REQUIRE(test_result);
    }

    // Run the uut for a bunch of cycles to fill the cache
    for (auto i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();

    WHEN("A packet with the same address is issued") {
      ::update_interface_discerner[&uut] = 0;
      auto repeat_test_result = mock_ul.issue(test);

      THEN("The issue is received") {
        REQUIRE(repeat_test_result);
      }

      // Run the uut for a bunch of cycles to fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The replacement policy is called with the address interface") {
        REQUIRE(::update_interface_discerner[&uut] == 3);
      }
    }
  }
}

SCENARIO("The simulator selects the cache fill function if it is available") {
  using namespace std::literals;
  auto [type, str] = GENERATE(table<access_type, std::string_view>({std::pair{access_type::LOAD, "load"sv}, std::pair{access_type::RFO, "RFO"sv}, std::pair{access_type::PREFETCH, "prefetch"sv}, std::pair{access_type::WRITE, "write"sv}, std::pair{access_type::TRANSLATION, "translation"sv}}));
  GIVEN("A cache with one element") {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 10;
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l2c}
      .name("443b-uut-"+std::string{str})
      .sets(1)
      .ways(1)
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .prefetch_activate(type)
      .offset_bits(champsim::data::bits{})
      .replacement<::fill_selection, lru>()
    };

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A packet with the is issued") {
      ::update_interface_discerner[&uut] = 0;
      ::fill_override_interface_discerner[&uut] = 0;
      decltype(mock_ul)::request_type test;
      test.address = champsim::address{0xdeadbeef};
      test.address = champsim::address{0xdeadbeef};
      test.cpu = 0;
      test.type = type;
      auto test_result = mock_ul.issue(test);

      THEN("The issue is received") {
        REQUIRE(test_result);
      }

      // Run the uut for a bunch of cycles to miss the cache
      for (uint64_t i = 0; i < 2*hit_latency; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The replacement policy is called with the address interface") {
        REQUIRE(::update_interface_discerner[&uut] == 1);
        REQUIRE(::fill_override_interface_discerner[&uut] == 0);
      }

      AND_WHEN("The cache is filled") {
        // Run the uut for a bunch of cycles to fill the cache
        for (auto i = 0; i < 100; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The replacement policy is called with the address interface") {
          REQUIRE(::update_interface_discerner[&uut] == 1);
          REQUIRE(::fill_override_interface_discerner[&uut] == 1);
        }
      }
    }
  }
}
