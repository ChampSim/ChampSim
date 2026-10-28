#include <catch.hpp>

#include "cache.h"
#include "defaults.hpp"
#include "matchers.hpp"
#include "mocks.hpp"

namespace
{
struct test_fixture {
  constexpr static long hit_latency = 7;
  constexpr static long miss_latency = 3;

  do_nothing_MRC mock_ll{miss_latency};
  to_rq_MRP mock_ul;
  CACHE uut;

  uint64_t id = 1;

  void operate_many_cycles()
  {
    std::array<champsim::operable*, 3> elements{{&uut, &mock_ll, &mock_ul}};

    for (auto i = 0; i < 100; ++i)
      for (auto elem : elements)
        elem->_operate();
  }

  test_fixture(uint32_t set, uint32_t way)
      : uut(champsim::modules::ModuleBuilder{"t408_cache", "DEFAULT_CACHE", champsim::defaults::default_l2c()}
                .add_parameter("mshr_size", static_cast<uint32_t>(8))
                .add_parameter("num_sets", static_cast<uint32_t>(set))
                .add_parameter("num_ways", static_cast<uint32_t>(way))
                .add_parameter("offset_bits", champsim::data::bits{champsim::modules::ModuleBuilder::globals().get_parameter<unsigned>("log2_block_size")})
                .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
                .add_parameter("max_tag_bandwidth", champsim::bandwidth::maximum_type{way})
                .add_parameter("max_fill_bandwidth", champsim::bandwidth::maximum_type{way}))
  {
    std::array<champsim::operable*, 3> elements{{&uut, &mock_ll, &mock_ul}};

    // Initialize the prefetching and replacement
    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }
  }

  void issue_sequence(std::vector<champsim::address> addrs)
  {
    mock_ll.addresses.clear();
    mock_ul.packets.clear();
    for (auto addr : addrs) {
      decltype(mock_ul)::request_type pkt;
      pkt.address = addr;
      pkt.is_translated = true;
      pkt.instr_id = id++;
      pkt.origin = champsim::origin{0, 0};
      pkt.type = access_type::WRITE;

      mock_ul.issue(pkt);
    }

    operate_many_cycles();
  }

  void assert_none_evicted(std::vector<champsim::address> addrs) { REQUIRE_THAT(mock_ll.addresses, champsim::test::DisjunctMatcher{addrs}); }

  void assert_all_returned(std::size_t count)
  {
    REQUIRE_THAT(mock_ul.packets, Catch::Matchers::SizeIs(count) && Catch::Matchers::AllMatch(champsim::test::ReturnedMatcher{hit_latency, 1}));
  }
};

auto strided_address_generator(champsim::address start, champsim::address::difference_type stride)
{
  return [start, stride]() mutable {
    auto retval = start;
    start += stride;
    return retval;
  };
}
} // namespace

SCENARIO("A cache set is not evicted by fills to the succeeding set")
{
  constexpr uint32_t num_sets = 16;
  constexpr uint32_t num_ways = 2;

  constexpr std::size_t num_seeds = num_ways;
  constexpr std::size_t num_tests = num_ways;

  champsim::address seed_base{0xbeef0000};
  champsim::address test_base{champsim::block_number{seed_base} + 1};
  champsim::address::difference_type set_diff = num_sets * champsim::modules::ModuleBuilder::globals().get_parameter<unsigned>("block_size");

  GIVEN("A cache with one set full")
  {
    ::test_fixture fixture{num_sets, num_ways};

    std::vector<champsim::address> seed_addresses{};
    std::generate_n(std::back_inserter(seed_addresses), num_seeds, ::strided_address_generator(seed_base, set_diff));

    fixture.issue_sequence(seed_addresses);

    WHEN("The first set is reaccessed")
    {
      fixture.issue_sequence(seed_addresses);

      THEN("The first set still hits all of its members") { fixture.assert_all_returned(num_seeds); }
    }

    WHEN("The next set is filled")
    {
      std::vector<champsim::address> test_addresses{};
      std::generate_n(std::back_inserter(test_addresses), num_tests, ::strided_address_generator(test_base, set_diff));

      fixture.issue_sequence(test_addresses);

      THEN("None of the first set is evicted") { fixture.assert_none_evicted(seed_addresses); }

      AND_WHEN("The first set is reaccessed")
      {
        fixture.issue_sequence(seed_addresses);

        THEN("The first set still hits all of its members") { fixture.assert_all_returned(num_seeds); }
      }
    }

    WHEN("The next set is thrashed")
    {
      std::vector<champsim::address> test_addresses{};
      std::generate_n(std::back_inserter(test_addresses), 2 * num_tests, ::strided_address_generator(test_base, set_diff));

      fixture.issue_sequence(test_addresses);

      THEN("None of the first set is evicted") { fixture.assert_none_evicted(seed_addresses); }

      AND_WHEN("The first set is reaccessed")
      {
        fixture.issue_sequence(seed_addresses);

        THEN("The first set still hits all of its members") { fixture.assert_all_returned(num_seeds); }
      }
    }
  }
}
