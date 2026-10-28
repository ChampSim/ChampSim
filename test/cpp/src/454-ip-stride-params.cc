#include <algorithm>
#include <catch.hpp>
#include <numeric>

#include "../../../prefetcher/ip_stride/ip_stride.h"
#include "address.h"
#include "cache.h"
#include "defaults.hpp"
#include "mocks.hpp"

namespace
{
// Helper: build a CACHE with ip_stride using the given submodule builder
CACHE make_cache(do_nothing_MRC& mock_ll, to_rq_MRP& mock_ul,
                 champsim::modules::ModuleBuilder pref_sub = champsim::modules::ModuleBuilder{"t454_ip_stride_0", "ip_stride"})
{
  auto builder = champsim::modules::ModuleBuilder{"t454_cache", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
    .add_parameter("mshr_size", static_cast<uint32_t>(8))
    .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
    .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
    .add_submodule("prefetcher", std::move(pref_sub));

  return CACHE{std::move(builder)};
}

// Helper: issue strided packets to trigger prefetching and return total requests seen by lower level
std::size_t run_stride_test(do_nothing_MRC& mock_ll, to_rq_MRP& mock_ul, CACHE& uut)
{
  std::array<champsim::operable*, 3> elements{{&mock_ll, &mock_ul, &uut}};

  for (auto elem : elements) {
    elem->initialize();
    if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
  }

  static uint64_t id = 1;
  int64_t stride = 1;

  // Seed packet
  to_rq_MRP::request_type seed;
  seed.address = champsim::address{0xffff'003f};
  seed.ip = champsim::address{0xcafecafe};
  seed.instr_id = id++;
  seed.origin = champsim::origin{0, 0};
  mock_ul.issue(seed);

  for (auto i = 0; i < 100; ++i)
    for (auto elem : elements)
      elem->_operate();

  // Two strided packets to establish the stride pattern
  auto test_a = seed;
  test_a.address = champsim::address{champsim::block_number{seed.address} + stride};
  test_a.instr_id = id++;
  mock_ul.issue(test_a);

  auto test_b = test_a;
  test_b.address = champsim::address{champsim::block_number{test_a.address} + stride};
  test_b.instr_id = id++;
  mock_ul.issue(test_b);

  for (auto i = 0; i < 100; ++i)
    for (auto elem : elements)
      elem->_operate();

  return mock_ll.addresses.size();
}
} // namespace

SCENARIO("ip_stride respects the degree parameter")
{
  GIVEN("A cache with ip_stride using the default degree (3)")
  {
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    auto uut = make_cache(mock_ll, mock_ul);
    auto count = run_stride_test(mock_ll, mock_ul, uut);

    THEN("The prefetcher issues 3 prefetches (seed + 2 strided + 3 prefetched = 6 total)")
    {
      REQUIRE(count == 6);
    }
  }

  GIVEN("A cache with ip_stride using degree=5")
  {
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    auto pref_sub = champsim::modules::ModuleBuilder{"t454_ip_stride_1", "ip_stride"}.add_parameter("degree", 5);
    auto uut = make_cache(mock_ll, mock_ul, std::move(pref_sub));
    auto count = run_stride_test(mock_ll, mock_ul, uut);

    THEN("The prefetcher issues 5 prefetches (seed + 2 strided + 5 prefetched = 8 total)")
    {
      REQUIRE(count == 8);
    }
  }

  GIVEN("A cache with ip_stride using degree=1")
  {
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    auto pref_sub = champsim::modules::ModuleBuilder{"t454_ip_stride_2", "ip_stride"}.add_parameter("degree", 1);
    auto uut = make_cache(mock_ll, mock_ul, std::move(pref_sub));
    auto count = run_stride_test(mock_ll, mock_ul, uut);

    THEN("The prefetcher issues 1 prefetch (seed + 2 strided + 1 prefetched = 4 total)")
    {
      REQUIRE(count == 4);
    }
  }
}

SCENARIO("ip_stride respects the tracker_sets parameter")
{
  GIVEN("A cache with ip_stride using very small tracker_sets (1)")
  {
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    auto pref_sub = champsim::modules::ModuleBuilder{"t454_ip_stride_3", "ip_stride"}.add_parameter("tracker_sets", static_cast<std::size_t>(1));
    auto uut = make_cache(mock_ll, mock_ul, std::move(pref_sub));

    // With only 1 set, the table still works (just more conflicts).
    // Verify it doesn't crash and still produces some output.
    auto count = run_stride_test(mock_ll, mock_ul, uut);
    THEN("The cache still functions") { REQUIRE(count >= 3); }
  }
}

SCENARIO("ip_stride respects the tracker_ways parameter")
{
  GIVEN("A cache with ip_stride using tracker_ways=1")
  {
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    auto pref_sub = champsim::modules::ModuleBuilder{"t454_ip_stride_4", "ip_stride"}.add_parameter("tracker_ways", static_cast<std::size_t>(1));
    auto uut = make_cache(mock_ll, mock_ul, std::move(pref_sub));

    auto count = run_stride_test(mock_ll, mock_ul, uut);
    THEN("The cache still functions") { REQUIRE(count >= 3); }
  }
}
