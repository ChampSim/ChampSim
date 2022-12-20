#include "catch.hpp"
#include "mocks.hpp"
#include "cache.h"
#include "champsim_constants.h"

SCENARIO("A prefetch can be issued that creates an MSHR") {
  GIVEN("An empty cache") {
    constexpr uint64_t hit_latency = 1;
    constexpr uint64_t fill_latency = 10;
    do_nothing_MRC mock_ll;
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"421-uut", 1, 1, 8, 32, fill_latency, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &mock_ll, CACHE::pprefetcherDno, CACHE::rreplacementDlru};

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

      uut_queues._operate();
      uut._operate();
      mock_ll._operate();

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
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"421-uut", 1, 1, 8, 32, fill_latency, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &mock_ll, CACHE::pprefetcherDno, CACHE::rreplacementDlru};

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

      uut_queues._operate();
      uut._operate();
      mock_ll._operate();

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
    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE uut{"421-uut", 1, 1, 8, 32, fill_latency, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &mock_ll, CACHE::pprefetcherDno, CACHE::rreplacementDlru};

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

      for (int i = 0; i < 1000; i++) {
        uut_queues._operate();
        uut._operate();
        mock_ll._operate();
      }

      // We ensure that the block fill the first level
      bool find = false;
      for (auto i: uut.block) if (find = (i.address == 0xdeadbeef)) break;

      THEN("The packet fill the cache") {
        REQUIRE(find);
      }
    }
  }
}

SCENARIO("A prefetch not fill the first level and fill the second level") {
  GIVEN("An empty cache") {
    constexpr uint64_t hit_latency = 1;
    constexpr uint64_t fill_latency = 10;
    do_nothing_MRC mock_ll;

    CACHE::NonTranslatingQueues uut_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};
    CACHE::NonTranslatingQueues uul_queues{1, 32, 32, 32, 0, hit_latency, LOG2_BLOCK_SIZE, false};

    CACHE uul{"421-uul", 2, 2, 8, 32, fill_latency, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uul_queues, &mock_ll, CACHE::pprefetcherDno, CACHE::rreplacementDlru};
    CACHE uut{"421-uut", 1, 1, 8, 32, fill_latency, 1, 1, 0, false, false, false, (1<<LOAD)|(1<<PREFETCH), uut_queues, &uul, CACHE::pprefetcherDno, CACHE::rreplacementDlru};

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

      for (int i = 0; i < 100; i++) {
        uut_queues._operate();
        uut._operate();
        uul_queues._operate();
        uul._operate();
        mock_ll._operate();
      }

      THEN("The packet do not fill the first level") {
        bool find_block = false;
        for (auto i: uut.block) if (find_block = (i.address == 0xdeadbeef)) break;
        REQUIRE(!find_block);
      }

      THEN("The packet fill the second level") {
        bool find_block = false;
        for (auto i: uul.block) if (find_block = (i.address == 0xdeadbeef)) break;
        REQUIRE(find_block);
      }
    }
  }
}
