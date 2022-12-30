#include "catch.hpp"
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

SCENARIO("A prefetch can be issued that creates an MSHR") {
  GIVEN("An empty cache") {
    constexpr uint64_t hit_latency = 1;
    constexpr uint64_t fill_latency = 10;
    do_nothing_MRC mock_ll;
    champsim::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"421a-uut", 1, 1, 8, 32, fill_latency, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &mock_ll, CACHE::pprefetcherDno, CACHE::rreplacementDlru};

    std::array<champsim::operable*, 3> elements{{&mock_ll, &uut_queues, &uut}};

    // Initialize the prefetching and replacement
    uut.impl_prefetcher_initialize();
    uut.impl_replacement_initialize();

    // Turn off warmup
    uut.warmup = false;
    uut_queues.warmup = false;
    uut.begin_phase();
    uut_queues.begin_phase();

    WHEN("A prefetch is issued with 'fill_this_level == true'") {
      auto seed_result = uut.prefetch_line(0xdeadbeef, true, 0);
      REQUIRE(seed_result);

      for (int i = 0; i < 2; ++i) {
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
    champsim::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"421b-uut", 1, 1, 8, 32, fill_latency, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &mock_ll, CACHE::pprefetcherDno, CACHE::rreplacementDlru};

    std::array<champsim::operable*, 3> elements{{&mock_ll, &uut_queues, &uut}};

    // Initialize the prefetching and replacement
    uut.impl_prefetcher_initialize();
    uut.impl_replacement_initialize();

    // Turn off warmup
    uut.warmup = false;
    uut_queues.warmup = false;
    uut.begin_phase();
    uut_queues.begin_phase();

    WHEN("A prefetch is issued with 'fill_this_level == false'") {
      auto seed_result = uut.prefetch_line(0xdeadbeef, false, 0);
      REQUIRE(seed_result);

      for (int i = 0; i < 2; ++i) {
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
    champsim::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"421c-uut", 1, 1, 8, 32, fill_latency, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &mock_ll, CACHE::pprefetcherDno, CACHE::rreplacementDlru};
    to_rq_MRP mock_ut{&uut};

    std::array<champsim::operable*, 4> elements{{&uut, &mock_ll, &mock_ut, &uut_queues}};

    // Initialize the prefetching and replacement
    uut.impl_prefetcher_initialize();
    uut.impl_replacement_initialize();

    // Turn off warmup
    uut.warmup = false;
    uut_queues.warmup = false;
    uut.begin_phase();
    uut_queues.begin_phase();

    WHEN("A prefetch is issued with 'fill_this_level == true'") {
      auto seed_result = uut.prefetch_line(0xdeadbeef, true, 0);
      REQUIRE(seed_result);

      for (uint64_t i = 0; i < 2*fill_latency; i++) 
        for (auto elem : elements) 
          elem->_operate();

      AND_WHEN("A packet with the same address is sent")
      {
        PACKET test;
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

    champsim::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    champsim::NonTranslatingQueues uul_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};

    CACHE uul{"421d-uul", 1, 1, 8, 32, fill_latency, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uul_queues, &mock_ll, CACHE::pprefetcherDno, CACHE::rreplacementDlru};
    CACHE uut{"421d-uut", 1, 1, 8, 32, fill_latency, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &uul, CACHE::pprefetcherDno, CACHE::rreplacementDlru};

    to_rq_MRP mock_ul{&uul};
    to_rq_MRP mock_ut{&uut};

    std::array<champsim::operable*, 7> elements{{&uul, &uut, &mock_ll, &mock_ut, &mock_ul, &uut_queues, &uul_queues}};

    // Initialize the prefetching and replacement
    uut.impl_prefetcher_initialize();
    uut.impl_replacement_initialize();
    uul.impl_prefetcher_initialize();
    uul.impl_replacement_initialize();

    // Turn off warmup
    uut.warmup = false;
    uut_queues.warmup = false;
    uut.begin_phase();
    uut_queues.begin_phase();

    uul.warmup = false;
    uul_queues.warmup = false;
    uul.begin_phase();
    uul_queues.begin_phase();

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

        PACKET test;
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
          REQUIRE(mock_ut.packets.back().return_time == mock_ut.packets.back().issue_time + 2*hit_latency + fill_latency);
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

        PACKET test;
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
          REQUIRE(mock_ul.packets.back().return_time == mock_ul.packets.back().issue_time + hit_latency);
        }
      }
    }
  }
}
