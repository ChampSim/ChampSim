#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"
#include "state_model_interface.h"
#include "modules.h"

#include <map>
#include <vector>

namespace 
{
  std::map<CACHE*, std::vector<test::state_model_request_interface>> state_model_request_state_collector;
  std::map<CACHE*, std::vector<test::state_model_response_interface>> state_model_response_state_collector;
}

struct update_state_collector : champsim::modules::state_model
{
  using state_model::state_model;
  using state_response = CACHE::state_response_type;
    
  state_response handle_request(champsim::address address, long set, access_type type, bool hit, uint32_t cpu)
  {
    auto usc_it = ::state_model_request_state_collector.try_emplace(intern_);
    usc_it.first->second.push_back({cpu, set, address, access_type{type}, hit});
    return state_response(); 
  }

  state_response handle_response(champsim::address address, long set, access_type type, uint32_t cpu)
  {
    auto usc_it = ::state_model_response_state_collector.try_emplace(intern_);
    usc_it.first->second.push_back({cpu, set, address, access_type{type}});
    return state_response(); 
  }
};

SCENARIO("The state model is triggered on a miss, eviction check, and fill") {
  using namespace std::literals;
  auto [type, str] = GENERATE(table<access_type, std::string_view>(
        {std::pair{access_type::LOAD, "load"sv}, 
        std::pair{access_type::RFO, "RFO"sv}, 
        std::pair{access_type::PREFETCH, "prefetch"sv}, 
        std::pair{access_type::WRITE, "write"sv}, 
        std::pair{access_type::TRANSLATION, "translation"sv}
        })
    );

  GIVEN("A single cache") {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    release_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("459a-uut-"+std::string{str})
      .sets(1)
      .ways(1)
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .prefetch_activate(type)
      .offset_bits(champsim::data::bits{})
      .state_model<update_state_collector>()
    };

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A " + std::string{str} + " is issued") {
      ::state_model_request_state_collector[&uut].clear();
      ::state_model_response_state_collector[&uut].clear();

      decltype(mock_ul)::request_type test;
      test.address = champsim::address{0xdeadbeef};
      test.is_translated = true;
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

      THEN("The state model handle request is called") {
        REQUIRE(std::size(::state_model_request_state_collector[&uut]) == 1);
      }

      AND_WHEN("The packet is returned") {
        mock_ll.release(test.address);

        // Run the uut for a bunch of cycles to fill the cache
        for (auto i = 0; i < 100; ++i)
          for (auto elem : elements)
            elem->_operate();


        THEN("The state model is twice called for filling - once for the eviction and once for the fill") {
          REQUIRE(std::size(::state_model_request_state_collector[&uut]) == 3);
        }

        THEN("The state model is called with information from the issued packet") {
          REQUIRE_THAT(::state_model_request_state_collector[&uut], Catch::Matchers::SizeIs(3));
          CHECK(::state_model_request_state_collector[&uut].at(0).cpu == test.cpu);
          CHECK(::state_model_request_state_collector[&uut].at(0).set == 0);
          CHECK(::state_model_request_state_collector[&uut].at(0).full_addr == test.address);
          CHECK(::state_model_request_state_collector[&uut].at(0).type == test.type);
          CHECK(::state_model_request_state_collector[&uut].at(0).hit == false);
        }
      }
    }
  }
}

SCENARIO("The state model is triggered on a hit") {
  using namespace std::literals;
  auto [type, str] = GENERATE(table<access_type, std::string_view>({std::pair{access_type::LOAD, "load"sv}, std::pair{access_type::RFO, "RFO"sv}, std::pair{access_type::PREFETCH, "prefetch"sv}, std::pair{access_type::WRITE, "write"sv}, std::pair{access_type::TRANSLATION, "translation"sv}}));
  GIVEN("A cache with one element") {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l2c}
      .name("459b-uut-"+std::string{str})
      .sets(1)
      .ways(1)
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
      .prefetch_activate(type)
      .offset_bits(champsim::data::bits{})
      .state_model<update_state_collector>()
    };

    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    decltype(mock_ul)::request_type test;
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
      ::state_model_request_state_collector[&uut].clear();
      auto repeat_test_result = mock_ul.issue(test);

      THEN("The issue is received") {
        REQUIRE(repeat_test_result);
      }

      // Run the uut for a bunch of cycles to fill the cache
      for (auto i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The state model request is called with information from the issued packet") {
        REQUIRE_THAT(::state_model_request_state_collector[&uut], Catch::Matchers::SizeIs(1));
        CHECK(::state_model_request_state_collector[&uut].at(0).cpu == test.cpu);
        CHECK(::state_model_request_state_collector[&uut].at(0).set == 0);
        CHECK(::state_model_request_state_collector[&uut].at(0).full_addr == test.address);
        CHECK(::state_model_request_state_collector[&uut].at(0).type == test.type);
        CHECK(::state_model_request_state_collector[&uut].at(0).hit == true);
      }

    }
  }
}
