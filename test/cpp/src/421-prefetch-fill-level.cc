#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"
#include "cache.h"
#include "champsim_constants.h"

SCENARIO("A prefetch can be issued that creates an MSHR") {
  GIVEN("An empty cache") {
    constexpr uint64_t hit_latency = 1;
    constexpr uint64_t fill_latency = 10;
    do_nothing_MRC mock_ll;
    CACHE uut{CACHE::Builder{champsim::defaults::default_l1d}
      .name("421a-uut")
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
    };

    std::array<champsim::operable*, 2> elements{{&mock_ll, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A prefetch is issued with 'fill_this_level == true'") {
      auto seed_result = uut.prefetch_line(0xdeadbeef, true, 0);
      REQUIRE(seed_result);

      for (int i = 0; i < 10; ++i) {
        for (auto elem : elements)
          elem->_operate();
      }

      THEN("The packet is forwarded and an MSHR is created") {
        REQUIRE(std::size(uut.MSHR) == 1);
        REQUIRE(mock_ll.packet_count() == 1);
      }
    }
  }
}


SCENARIO("A prefetch can be issued without creating an MSHR") {
  GIVEN("An empty cache") {
    constexpr uint64_t hit_latency = 1;
    constexpr uint64_t fill_latency = 10;
    do_nothing_MRC mock_ll;
    CACHE uut{CACHE::Builder{champsim::defaults::default_l1d}
      .name("421b-uut")
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
    };

    std::array<champsim::operable*, 2> elements{{&mock_ll, &uut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A prefetch is issued with 'fill_this_level == false'") {
      auto seed_result = uut.prefetch_line(0xdeadbeef, false, 0);
      REQUIRE(seed_result);

      for (int i = 0; i < 10; ++i) {
        for (auto elem : elements)
          elem->_operate();
      }

      THEN("The packet is forwarded without an MSHR being created") {
        REQUIRE(std::empty(uut.MSHR));
        REQUIRE(mock_ll.packet_count() == 1);
      }
    }
  }
}

SCENARIO("A prefetch fill the first level") {
  GIVEN("An empty cache") {
    constexpr uint64_t hit_latency = 1;
    constexpr uint64_t fill_latency = 10;
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ut;
    CACHE uut{CACHE::Builder{champsim::defaults::default_l1d}
      .name("421c-uut")
      .upper_levels({&mock_ut.queues})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
    };

    std::array<champsim::operable*, 3> elements{{&uut, &mock_ll, &mock_ut}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A prefetch is issued with 'fill_this_level == true'") {
      auto seed_result = uut.prefetch_line(0xdeadbeef, true, 0);
      REQUIRE(seed_result);

      for (uint64_t i = 0; i < 2*fill_latency; i++) 
        for (auto elem : elements) 
          elem->_operate();

      AND_WHEN("A packet with the same address is sent")
      {
        decltype(mock_ut)::request_type test;
        test.address = 0xdeadbeef;
        test.cpu = 0;

        auto test_result = mock_ut.issue(test);

        THEN("The issue is accepted") {
          REQUIRE(test_result);
        }

        for (uint64_t i = 0; i < 2*hit_latency; i++)
          for (auto elem : elements) 
            elem->_operate();

        THEN("The packet hits the cache") {
          REQUIRE(mock_ut.packets.back().return_time == mock_ut.packets.back().issue_time + hit_latency);
        }
      }
    }
  }
}

SCENARIO("A prefetch not fill the first level and fill the second level") {
  GIVEN("An empty cache") {
    constexpr uint64_t hit_latency = 3;
    constexpr uint64_t fill_latency = 10;
    do_nothing_MRC mock_ll;

    champsim::channel uul_queues{};

    to_rq_MRP mock_ul;
    to_rq_MRP mock_ut;

    CACHE uul{CACHE::Builder{champsim::defaults::default_l1d}
      .name("421d-uul")
      .upper_levels({{&mock_ul.queues, &uul_queues}})
      .lower_level(&mock_ll.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
    };
    CACHE uut{CACHE::Builder{champsim::defaults::default_l1d}
      .name("421d-uut")
      .upper_levels({&mock_ut.queues})
      .lower_level(&uul_queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
    };

    std::array<champsim::operable*, 5> elements{{&uul, &uut, &mock_ll, &mock_ut, &mock_ul}};

    for (auto elem : elements) {
      elem->initialize();
      elem->warmup = false;
      elem->begin_phase();
    }

    WHEN("A prefetch is issued with 'fill_this_level == false'") {
      auto seed_result = uut.prefetch_line(0xdeadbeef, false, 0);
      REQUIRE(seed_result);

      for (uint64_t i = 0; i < 2*(hit_latency + fill_latency + 1); i++) {
        for (auto elem : elements) 
          elem->_operate();
      }

      AND_WHEN("A packet with the same address is sent")
      {
        mock_ut.packets.clear();

        decltype(mock_ut)::request_type test;
        test.address = 0xdeadbeef;
        test.is_translated = true;
        test.cpu = 0;
        test.instr_id = 1;

        auto test_result = mock_ut.issue(test);

        THEN("The issue is accepted") {
          REQUIRE(test_result);
        }

        for (uint64_t i = 0; i < 6*(hit_latency+fill_latency); i++) {
          for (auto elem : elements) 
            elem->_operate();
        }

        THEN("The packet doesn't hits the first level of cache") {
          REQUIRE(std::size(mock_ut.packets) == 1);

          // The packet return time should be: issue time + hit_latency L2C + hit_latency L1D + fill latency L1D
          REQUIRE(mock_ut.packets.back().return_time >= mock_ut.packets.back().issue_time + 2*hit_latency + fill_latency);
        }
      }
    }

    WHEN("Another prefetch is issued with 'fill_this_level == false'") {
      auto seed_result = uut.prefetch_line(0xbebacafe, false, 0);
      REQUIRE(seed_result);

      for (uint64_t i = 0; i < 6*fill_latency; i++) {
        for (auto elem : elements) 
          elem->_operate();
      }

      AND_WHEN("A packet with the same address is sent")
      {
        mock_ul.packets.clear();

        decltype(mock_ul)::request_type test;
        test.address = 0xbebacafe;
        test.is_translated = true;
        test.cpu = 0;
        test.instr_id = 2;

        auto test_result = mock_ul.issue(test);

        THEN("The issue is accepted") {
          REQUIRE(test_result);
        }

        for (uint64_t i = 0; i < 6*(hit_latency+fill_latency); i++) {
          for (auto elem : elements) 
            elem->_operate();
        }

        THEN("The packet hits the second level of cache") {
          REQUIRE(std::size(mock_ul.packets) == 1);

          // The packet return time should be: issue time + hit_latency L2C
          REQUIRE(mock_ul.packets.back().return_time >= mock_ul.packets.back().issue_time + hit_latency);
        }
      }
    }
  }
}
