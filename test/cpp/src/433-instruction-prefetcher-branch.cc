#include <catch.hpp>

#include <map>

#include "cache.h"
#include "defaults.hpp"
#include "ooo_cpu.h"
#include "mocks.hpp"
#include "instr.h"

namespace
{
  std::map<CACHE*, std::vector<champsim::address>> branch_address_collector;

  struct branch_collector : public champsim::modules::prefetcher
  {
    using prefetcher::prefetcher;

    uint32_t prefetcher_cache_operate(champsim::address, champsim::address, uint8_t, bool, access_type, uint32_t metadata_in)
    {
      return metadata_in;
    }

    uint32_t prefetcher_cache_fill(champsim::address, long, long, uint8_t, champsim::address, uint32_t metadata_in)
    {
      return metadata_in;
    }

    void prefetcher_branch_operate(champsim::address ip, uint8_t, champsim::address)
    {
      auto it = branch_address_collector.try_emplace(intern_);
      it.first->second.push_back(ip);
    }
  };
}

SCENARIO("An instruction prefetcher's branch hook is called from the core") {
  GIVEN("A core and an instruction cache with a prefetcher") {
    do_nothing_MRC mock_L1D, mock_tlb;

    champsim::channel core_to_icache{};
    do_nothing_MRC mock_ll;
    CACHE uut_l1i{champsim::cache_builder{champsim::defaults::default_l1i}
      .name("430-uut")
      .upper_levels({&core_to_icache, &mock_tlb.queues})
      .lower_level(&mock_ll.queues)
      .lower_translate(&mock_tlb.queues)
      .prefetcher<::branch_collector>()
    };

    O3_CPU uut{champsim::core_builder{champsim::defaults::default_core}
      .fetch_queues(&core_to_icache)
      .data_queues(&mock_L1D.queues)
      .l1i(&uut_l1i)
    };

    uut.warmup = false;
    std::array<champsim::operable*, 5> elements{{&uut, &uut_l1i, &mock_L1D, &mock_ll, &mock_tlb}};

    WHEN("A branch is issued") {
      ::branch_address_collector.insert_or_assign(&uut_l1i, std::vector<champsim::address>{});

      constexpr champsim::address branch_address{0xdeadbeef};
      uut.input_queue.push_back(champsim::test::branch_instruction_with_ip(branch_address));

      // Run the uut for a bunch of cycles to fill the cache
      for (auto i = 0; i < 10; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The prefetcher is called") {
        REQUIRE_THAT(::branch_address_collector.at(&uut_l1i), Catch::Matchers::Contains(branch_address));
      }
    }
  }
}

SCENARIO("A moved-to instruction prefetcher's branch hook is called from a core") {
  GIVEN("A core and an instruction cache with a prefetcher") {
    do_nothing_MRC mock_L1D, mock_tlb;

    champsim::channel core_to_icache{};
    do_nothing_MRC mock_ll;
    CACHE uut_l1i_source{champsim::cache_builder{champsim::defaults::default_l1i}
      .name("430-uut")
      .upper_levels({&core_to_icache, &mock_tlb.queues})
      .lower_level(&mock_ll.queues)
      .lower_translate(&mock_tlb.queues)
      .prefetcher<::branch_collector>()
    };

    O3_CPU uut_source{champsim::core_builder{champsim::defaults::default_core}
      .fetch_queues(&core_to_icache)
      .data_queues(&mock_L1D.queues)
      .l1i(&uut_l1i_source)
    };

    O3_CPU uut{std::move(uut_source)};
    CACHE uut_l1i{std::move(uut_l1i_source)};

    uut.warmup = false;
    std::array<champsim::operable*, 5> elements{{&uut, &uut_l1i, &mock_L1D, &mock_ll, &mock_tlb}};

    WHEN("A branch is issued") {
      ::branch_address_collector.insert_or_assign(&uut_l1i, std::vector<champsim::address>{});

      constexpr champsim::address branch_address{0xdeadbeef};
      uut.input_queue.push_back(champsim::test::branch_instruction_with_ip(branch_address));

      // Run the uut for a bunch of cycles to fill the cache
      for (auto i = 0; i < 10; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The prefetcher is called") {
        REQUIRE_THAT(::branch_address_collector.at(&uut_l1i), Catch::Matchers::Contains(branch_address));
      }
    }
  }
}
