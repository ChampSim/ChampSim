#include <catch.hpp>

#include "../replacement/ship/ship.h"
#include "cache.h"
#include "defaults.hpp"
#include "mocks.hpp"
#include "modules.h"

TEST_CASE("SHIP sampler matches at cache block granularity")
{
  /*
   * The SHIP sampler compares addresses at cache block granularity so that
   * two addresses in the same cache block are treated as the same entry.
   *
   * With BLOCK_SIZE=64 (OFFSET_BITS=6):
   *   block_number{address{0}}  == block_number{address{32}}  (same block)
   *   block_number{address{0}}  != block_number{address{512}} (different blocks)
   *
   * Observable behavior through the function interface:
   *   - Same-block accesses produce sampler hits, decrementing SHCT. When SHCT
   *     is below its maximum, replacement_cache_fill assigns rrpv = maxRRPV - 1.
   *   - Different-block accesses produce sampler misses. When an unused sampler
   *     entry is evicted, SHCT is incremented. When SHCT reaches its maximum,
   *     replacement_cache_fill assigns rrpv = maxRRPV, making the line the
   *     preferred eviction victim in find_victim.
   */
  do_nothing_MRC mock_ll;
  to_rq_MRP mock_ul;
  CACHE cache{champsim::modules::ModuleBuilder{"445-ship-tag-test", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
    .add_parameter("mshr_size", static_cast<uint32_t>(8))
    .add_parameter("num_sets", static_cast<uint32_t>(8))
    .add_parameter("num_ways", static_cast<uint32_t>(8))
    .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
    .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
    .add_parameter("offset_bits", champsim::data::bits{6}) // BLOCK_SIZE=64 → OFFSET_BITS=6
    .add_submodule("replacement", champsim::modules::ModuleBuilder{"445-ship-tag-testship", "ship"})
  };

  auto* ship_repl = dynamic_cast<ship*>(champsim::modules::replacement::get_instance<champsim::modules::replacement>("445-ship-tag-testship"));
  REQUIRE(ship_repl != nullptr);
  auto& uut = *ship_repl;

  champsim::address test_ip{100};
  champsim::address baseline_ip{200};

  // Set 0 is a sampled set (category 0 with 8 sets, sample_rate=4).
  // Set 1 is not sampled, used for observing RRPV effects via find_victim.
  constexpr long sampler_set = 0;
  constexpr long observe_set = 1;

  SECTION("Same-block addresses are recognized as a sampler hit")
  {
    // Push SHCT for test_ip to its maximum via many distinct-block accesses.
    // With 8 sampler ways, the first 8 accesses fill the sampler; subsequent
    // accesses evict unused entries (ip=test_ip), incrementing SHCT each time.
    for (int i = 0; i < 15; ++i) {
      champsim::address addr{static_cast<uint64_t>(i * 512)};
      uut.update_replacement_state(champsim::origin{0, 0}, sampler_set, 0, addr, test_ip, champsim::address{}, access_type::LOAD, 0);
    }

    // Access addr 0: sampler miss (block 0 was evicted from sampler during pumping)
    champsim::address addr1{0};
    uut.update_replacement_state(champsim::origin{0, 0}, sampler_set, 0, addr1, test_ip, champsim::address{}, access_type::LOAD, 0);

    // Access addr 32 (same cache block as addr 0, BLOCK_SIZE=64):
    // Correct implementation recognizes this as a sampler HIT → SHCT decremented below max.
    // A buggy shamt would treat it as a different entry (miss) → SHCT stays at max.
    champsim::address addr2{32};
    uut.update_replacement_state(champsim::origin{0, 0}, sampler_set, 0, addr2, test_ip, champsim::address{}, access_type::LOAD, 0);

    // Fill all ways of the observation set:
    // ways 0-6 with baseline_ip (SHCT=0 → rrpv = maxRRPV - 1)
    // way 7 with test_ip
    for (long w = 0; w < 7; ++w)
      uut.replacement_cache_fill(champsim::origin{0, 0}, observe_set, w, champsim::address{static_cast<uint64_t>((w + 1) * 0x1000)},
                                 baseline_ip, champsim::address{}, access_type::LOAD);
    uut.replacement_cache_fill(champsim::origin{0, 0}, observe_set, 7, champsim::address{0x8000},
                               test_ip, champsim::address{}, access_type::LOAD);

    // With SHCT below max (correct impl), way 7 gets the same rrpv as baseline ways.
    // find_victim should NOT preferentially select way 7.
    auto victim = uut.find_victim(champsim::origin{0, 0}, 0, observe_set, nullptr, test_ip, champsim::address{}, access_type::LOAD);
    CHECK(victim != 7);
  }

  SECTION("Different-block addresses are recognized as a sampler miss")
  {
    // Access 15 distinct blocks: each unique block beyond the sampler capacity (8)
    // evicts an unused entry, incrementing SHCT. After enough evictions SHCT reaches max.
    for (int i = 0; i < 15; ++i) {
      champsim::address addr{static_cast<uint64_t>(i * 512)};
      uut.update_replacement_state(champsim::origin{0, 0}, sampler_set, 0, addr, test_ip, champsim::address{}, access_type::LOAD, 0);
    }

    // Fill all ways of the observation set:
    // ways 0-6 with baseline_ip (SHCT=0 → rrpv = maxRRPV - 1)
    // way 7 with test_ip (SHCT at max → rrpv = maxRRPV)
    for (long w = 0; w < 7; ++w)
      uut.replacement_cache_fill(champsim::origin{0, 0}, observe_set, w, champsim::address{static_cast<uint64_t>((w + 1) * 0x1000)},
                                 baseline_ip, champsim::address{}, access_type::LOAD);
    uut.replacement_cache_fill(champsim::origin{0, 0}, observe_set, 7, champsim::address{0x8000},
                               test_ip, champsim::address{}, access_type::LOAD);

    // With SHCT at max, way 7 gets rrpv = maxRRPV (higher than baseline ways).
    // find_victim should select way 7 as the preferred eviction victim.
    auto victim = uut.find_victim(champsim::origin{0, 0}, 0, observe_set, nullptr, test_ip, champsim::address{}, access_type::LOAD);
    CHECK(victim == 7);
  }
}
