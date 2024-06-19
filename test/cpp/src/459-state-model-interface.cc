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

  state_response handle_request(uint32_t cpu, champsim::address address, long set, access_type type, bool hit)
  {
    auto usc_it = ::state_model_request_state_collector.try_emplace(intern_);
    usc_it.first->second.push_back({cpu, set, address, access_type{type}, hit});
    return state_response(); 
  }

  state_response handle_response(uint32_t cpu, champsim::address address, long set, access_type type)
  {
    auto usc_it = ::state_model_response_state_collector.try_emplace(intern_);
    usc_it.first->second.push_back({cpu, set, address, access_type{type}});
    return state_response(); 
  }
};

SCENARIO("The state model is triggered on a miss") {
  using namespace std::literals;
  auto [type, str] = GENERATE(table<access_type, std::string_view>({std::pair{access_type::LOAD, "load"sv}, std::pair{access_type::RFO, "RFO"sv}, std::pair{access_type::PREFETCH, "prefetch"sv}, std::pair{access_type::WRITE, "write"sv}, std::pair{access_type::TRANSLATION, "translation"sv}}));
  GIVEN("A single cache") {
    constexpr uint64_t hit_latency = 2;
    constexpr uint64_t fill_latency = 2;
    release_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::cache_builder{champsim::defaults::default_l1d}
      .name("442a-uut-"+std::string{str})
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

      THEN("The replacement policy is not called") {
        REQUIRE(std::size(::state_model_request_state_collector[&uut]) == 0);
      }

      AND_WHEN("The packet is returned") {
        mock_ll.release(test.address);

        // Run the uut for a bunch of cycles to fill the cache
        for (auto i = 0; i < 100; ++i)
          for (auto elem : elements)
            elem->_operate();

        THEN("The replacement policy is called with information from the issued packet") {
          REQUIRE_THAT(::state_model_request_state_collector[&uut], Catch::Matchers::SizeIs(1));
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

//SCENARIO("The replacement policy is triggered on a hit") {
//  using namespace std::literals;
//  auto [type, str] = GENERATE(table<access_type, std::string_view>({std::pair{access_type::LOAD, "load"sv}, std::pair{access_type::RFO, "RFO"sv}, std::pair{access_type::PREFETCH, "prefetch"sv}, std::pair{access_type::WRITE, "write"sv}, std::pair{access_type::TRANSLATION, "translation"sv}}));
//  GIVEN("A cache with one element") {
//    constexpr uint64_t hit_latency = 2;
//    constexpr uint64_t fill_latency = 2;
//    do_nothing_MRC mock_ll;
//    to_rq_MRP mock_ul;
//    CACHE uut{champsim::cache_builder{champsim::defaults::default_l2c}
//      .name("442b-uut-"+std::string{str})
//      .sets(1)
//      .ways(1)
//      .upper_levels({&mock_ul.queues})
//      .lower_level(&mock_ll.queues)
//      .hit_latency(hit_latency)
//      .fill_latency(fill_latency)
//      .prefetch_activate(type)
//      .offset_bits(champsim::data::bits{})
//      .replacement<update_state_collector, lru>()
//    };
//
//    std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};
//
//    for (auto elem : elements) {
//      elem->initialize();
//      elem->warmup = false;
//      elem->begin_phase();
//    }
//
//    decltype(mock_ul)::request_type test;
//    test.address = champsim::address{0xdeadbeef};
//    test.cpu = 0;
//    test.type = type;
//    auto test_result = mock_ul.issue(test);
//
//    THEN("The issue is received") {
//      REQUIRE(test_result);
//    }
//
//    // Run the uut for a bunch of cycles to fill the cache
//    for (auto i = 0; i < 100; ++i)
//      for (auto elem : elements)
//        elem->_operate();
//
//    WHEN("A packet with the same address is issued") {
//      test::state_model_update_state_collector[&uut].clear();
//      auto repeat_test_result = mock_ul.issue(test);
//
//      THEN("The issue is received") {
//        REQUIRE(repeat_test_result);
//      }
//
//      // Run the uut for a bunch of cycles to fill the cache
//      for (auto i = 0; i < 100; ++i)
//        for (auto elem : elements)
//          elem->_operate();
//
//      THEN("The replacement policy is called with information from the issued packet") {
//        REQUIRE_THAT(test::state_model_update_state_collector[&uut], Catch::Matchers::SizeIs(1));
//        CHECK(test::state_model_update_state_collector[&uut].at(0).cpu == test.cpu);
//        CHECK(test::state_model_update_state_collector[&uut].at(0).set == 0);
//        CHECK(test::state_model_update_state_collector[&uut].at(0).way == 0);
//        CHECK(test::state_model_update_state_collector[&uut].at(0).full_addr == test.address);
//        CHECK(test::state_model_update_state_collector[&uut].at(0).type == test.type);
//        CHECK(test::state_model_update_state_collector[&uut].at(0).hit == true);
//      }
//    }
//  }
//}
//
//SCENARIO("The replacement policy notes the correct eviction information") {
//  GIVEN("An empty cache") {
//    constexpr uint64_t hit_latency = 2;
//    constexpr uint64_t fill_latency = 2;
//    do_nothing_MRC mock_ll;
//    to_wq_MRP mock_ul_seed;
//    to_rq_MRP mock_ul_test;
//    CACHE uut{champsim::cache_builder{champsim::defaults::default_l2c}
//      .name("442c-uut")
//      .sets(1)
//      .ways(1)
//      .upper_levels({&mock_ul_seed.queues, &mock_ul_test.queues})
//      .lower_level(&mock_ll.queues)
//      .hit_latency(hit_latency)
//      .fill_latency(fill_latency)
//      .prefetch_activate(access_type::LOAD)
//      .offset_bits(0)
//      .replacement<update_state_collector, lru>()
//    };
//
//    std::array<champsim::operable*, 4> elements{{&mock_ll, &mock_ul_seed, &mock_ul_test, &uut}};
//
//    for (auto elem : elements) {
//      elem->initialize();
//      elem->warmup = false;
//      elem->begin_phase();
//    }
//
//    WHEN("A packet is issued") {
//      uint64_t id = 0;
//      decltype(mock_ul_seed)::request_type seed;
//      seed.address = champsim::address{0xdeadbeef};
//      seed.v_address = champsim::address{0xdeadbeef};
//      seed.cpu = 0;
//      seed.instr_id = id++;
//      seed.type = access_type::WRITE;
//      auto seed_result = mock_ul_seed.issue(seed);
//
//      THEN("The issue is received") {
//        REQUIRE(seed_result);
//      }
//
//      // Run the uut for a bunch of cycles to fill the cache
//      for (auto i = 0; i < 100; ++i)
//        for (auto elem : elements)
//          elem->_operate();
//
//      AND_WHEN("A packet with a different address is issued") {
//        test::state_model_update_state_collector[&uut].clear();
//
//        decltype(mock_ul_test)::request_type test = seed;
//        test.address = 0xcafebabe;
//        test.instr_id = id++;
//        test.type = access_type::LOAD;
//        auto test_result = mock_ul_test.issue(test);
//
//        // Process the miss
//        for (uint64_t i = 0; i < 100; ++i)
//          for (auto elem : elements)
//            elem->_operate();
//
//        THEN("The issue is received") {
//          REQUIRE(test_result);
//          REQUIRE(mock_ll.packet_count() >= 1);
//          REQUIRE(mock_ll.addresses.at(0) == test.address);
//        }
//
//        for (uint64_t i = 0; i < 2*(fill_latency+hit_latency); ++i)
//          for (auto elem : elements)
//            elem->_operate();
//
//        THEN("An eviction occurred") {
//          REQUIRE_THAT(mock_ll.addresses, Catch::Matchers::SizeIs(2));
//          REQUIRE(mock_ll.addresses.at(1) == seed.address);
//        }
//
//        THEN("The replacement policy is called with information from the evicted packet") {
//          REQUIRE_THAT(test::state_model_update_state_collector[&uut], Catch::Matchers::SizeIs(1));
//          CHECK(test::state_model_update_state_collector[&uut].back().cpu == test.cpu);
//          CHECK(test::state_model_update_state_collector[&uut].back().set == 0);
//          CHECK(test::state_model_update_state_collector[&uut].back().way == 0);
//          CHECK(test::state_model_update_state_collector[&uut].back().full_addr == test.address);
//          CHECK(test::state_model_update_state_collector[&uut].back().type == access_type::LOAD);
//          CHECK(test::state_model_update_state_collector[&uut].back().victim_addr == seed.address);
//          CHECK(test::state_model_update_state_collector[&uut].back().hit == false);
//        }
//      }
//    }
//  }
//}
