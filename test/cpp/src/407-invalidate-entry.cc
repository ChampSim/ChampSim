// TODO - Rewrite this with the new invalidation flow

//#include <catch.hpp>
//#include "mocks.hpp"
//#include "defaults.hpp"
//#include "cache.h"
//#include "champsim_constants.h"
//
//SCENARIO("The lower level can invalidate an entry in the upper level") {
//  GIVEN("A cache with one filled entry") {
//    constexpr uint64_t hit_latency = 4;
//    constexpr uint64_t miss_latency = 3;
//    constexpr uint64_t fill_latency = 2;
//
//    do_nothing_MRC mock_ll{miss_latency};
//    to_rq_MRP mock_ul;
//
//    CACHE uut{champsim::cache_builder{champsim::defaults::default_l1d}
//      .name("407a-uut")
//      .upper_levels({&mock_ul.queues})
//      .lower_level(&mock_ll.queues)
//      .hit_latency(hit_latency)
//      .fill_latency(fill_latency)
//    };
//
//    std::array<champsim::operable*, 3> elements{{&uut, &mock_ll, &mock_ul}};
//
//    // Initialize the prefetching and replacement
//    for (auto elem : elements) {
//      elem->initialize();
//      elem->warmup = false;
//      elem->begin_phase();
//    }
//
//    // Create a seed packet
//    static uint64_t id = 1;
//    decltype(mock_ul)::request_type seed;
//    seed.address = 0xdeadbeef;
//    seed.is_translated = true;
//    seed.instr_id = id++;
//    seed.cpu = 0;
//    seed.type = access_type::LOAD;
//
//    // Issue it to the uut
//    auto seed_result = mock_ul.issue(seed);
//    THEN("This issue is received") {
//      REQUIRE(seed_result);
//    }
//
//    // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
//    for (auto i = 0; i < 100; ++i)
//      for (auto elem : elements)
//        elem->_operate();
//
//    WHEN("An invalidation request is issued") {
//      mock_ll.invalidate(seed.address);
//
//      // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
//      for (auto i = 0; i < 100; ++i)
//        for (auto elem : elements)
//          elem->_operate();
//
//      AND_WHEN("The seed packet is re-issued") {
//        auto reseed = seed;
//        reseed.instr_id = id++;
//
//        auto reseed_result = mock_ul.issue(reseed);
//        THEN("This issue is received") {
//          REQUIRE(reseed_result);
//        }
//
//        for (uint64_t i = 0; i < 100; ++i)
//          for (auto elem : elements)
//            elem->_operate();
//
//        THEN("The re-issue misses the cache") {
//          REQUIRE_THAT(mock_ul.packets, Catch::Matchers::SizeIs(2));
//          REQUIRE(mock_ul.packets.back().return_time - mock_ul.packets.back().issue_time == (fill_latency + miss_latency + hit_latency + 1)); // +1 due to ordering of elements
//        }
//      }
//    }
//  }
//}
//
//SCENARIO("The lower level can invalidate an entry in the upper level of its upper level") {
//  GIVEN("A cache with one filled entry") {
//    constexpr uint64_t hit_latency = 4;
//    constexpr uint64_t miss_latency = 3;
//    constexpr uint64_t fill_latency = 2;
//
//    do_nothing_MRC mock_ll{miss_latency};
//    to_rq_MRP mock_ul;
//
//    champsim::channel upper_to_lower{};
//
//    CACHE upper{champsim::cache_builder{champsim::defaults::default_l1d}
//      .name("407b-upper")
//      .upper_levels({&mock_ul.queues})
//      .lower_level(&upper_to_lower)
//      .hit_latency(hit_latency)
//      .fill_latency(fill_latency)
//    };
//
//    CACHE lower{champsim::cache_builder{champsim::defaults::default_l2c}
//      .name("407b-lower")
//      .upper_levels({&upper_to_lower})
//      .lower_level(&mock_ll.queues)
//      .hit_latency(hit_latency)
//      .fill_latency(fill_latency)
//    };
//
//    std::array<champsim::operable*, 4> elements{{&upper, &lower, &mock_ll, &mock_ul}};
//
//    // Initialize the prefetching and replacement
//    for (auto elem : elements) {
//      elem->initialize();
//      elem->warmup = false;
//      elem->begin_phase();
//    }
//
//    // Create a seed packet
//    static uint64_t id = 1;
//    decltype(mock_ul)::request_type seed;
//    seed.address = 0xdeadbeef;
//    seed.is_translated = true;
//    seed.instr_id = id++;
//    seed.cpu = 0;
//    seed.type = access_type::LOAD;
//
//    // Issue it to the uut
//    auto seed_result = mock_ul.issue(seed);
//    THEN("This issue is received") {
//      REQUIRE(seed_result);
//    }
//
//    // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
//    for (auto i = 0; i < 100; ++i)
//      for (auto elem : elements)
//        elem->_operate();
//
//    WHEN("An invalidation request is issued") {
//      mock_ll.invalidate(seed.address);
//
//      // Run the uut for a bunch of cycles to clear it out of the RQ and fill the cache
//      for (auto i = 0; i < 100; ++i)
//        for (auto elem : elements)
//          elem->_operate();
//
//      AND_WHEN("The seed packet is re-issued") {
//        auto reseed = seed;
//        reseed.instr_id = id++;
//
//        auto reseed_result = mock_ul.issue(reseed);
//        THEN("This issue is received") {
//          REQUIRE(reseed_result);
//        }
//
//        for (uint64_t i = 0; i < 100; ++i)
//          for (auto elem : elements)
//            elem->_operate();
//
//        THEN("The re-issue misses the cache") {
//          REQUIRE_THAT(mock_ul.packets, Catch::Matchers::SizeIs(2));
//          REQUIRE(mock_ul.packets.back().return_time - mock_ul.packets.back().issue_time == (2*fill_latency + miss_latency + 2*hit_latency + 2)); // +2 due to ordering of elements
//        }
//      }
//    }
//  }
//}
//
