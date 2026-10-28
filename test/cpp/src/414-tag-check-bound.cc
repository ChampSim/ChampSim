#include <catch.hpp>

#include "defaults.hpp"
#include "mocks.hpp"

TEST_CASE("Tag checks do not break when translation misses back up")
{
  constexpr uint64_t hit_latency = 1;
  constexpr uint64_t fill_latency = 3;
  release_MRC mock_translator;
  do_nothing_MRC mock_ll;
  to_rq_MRP mock_ul{[](auto x, auto y) {
    return x.v_address == y.v_address;
  }};
  CACHE uut{champsim::modules::ModuleBuilder{"t414_cache_0", "DEFAULT_CACHE", champsim::defaults::default_l2c()}
                .add_parameter("mshr_size", static_cast<uint32_t>(8))
                .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                .add_parameter("lower_translate", static_cast<champsim::modules::channel_module*>(&mock_translator.queues))
                .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
                .add_parameter("fill_latency", static_cast<uint64_t>(fill_latency))};

  std::array<champsim::operable*, 4> elements{{&uut, &mock_ll, &mock_ul, &mock_translator}};

  for (auto elem : elements) {
    elem->initialize();
    if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
  }

  std::array<champsim::page_number, 12> addresses;
  std::iota(std::begin(addresses), std::end(addresses), champsim::page_number{0xdeadb});

  std::vector<decltype(mock_ul)::request_type> packets;
  std::transform(std::begin(addresses), std::end(addresses), std::back_inserter(packets), [](auto addr) {
    // Create a test packet
    decltype(mock_ul)::request_type test;
    test.address = champsim::address{addr};
    test.v_address = test.address;
    test.is_translated = false;
    test.origin = champsim::origin{0, 0};
    return test;
  });

  for (const auto& pkt : packets)
    mock_ul.issue(pkt);

  for (int i = 0; i < 100; ++i) {
    for (auto elem : elements)
      elem->_operate();
  }

  mock_translator.release_all();

  for (int i = 0; i < 100; ++i) {
    for (auto elem : elements)
      elem->_operate();
  }

  SUCCEED();
}

TEST_CASE("Backed up translation misses do not prevent translated packets from advancing")
{
  constexpr uint64_t hit_latency = 1;
  constexpr uint64_t fill_latency = 3;
  champsim::channel refusal_channel{champsim::modules::ModuleBuilder{"t414_refusal_channel", "DEFAULT_CHANNEL", champsim::defaults::default_channel()}.add_parameter("rq_size", static_cast<std::size_t>(0)).add_parameter("wq_size", static_cast<std::size_t>(0)).add_parameter("pq_size", static_cast<std::size_t>(0))};
  do_nothing_MRC mock_ll;
  to_rq_MRP seed_ul{[](auto x, auto y) {
    return x.v_address == y.v_address;
  }};
  to_rq_MRP mock_ul{[](auto x, auto y) {
    return x.v_address == y.v_address;
  }};
  CACHE uut{champsim::modules::ModuleBuilder{"t414_cache_1", "DEFAULT_CACHE", champsim::defaults::default_l2c()}
                .add_parameter("mshr_size", static_cast<uint32_t>(8))
                .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&seed_ul.queues, &mock_ul.queues})
                .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                .add_parameter("lower_translate", static_cast<champsim::modules::channel_module*>(&refusal_channel))
                .add_parameter("mshr_size", static_cast<uint32_t>(1))
                .add_parameter("max_tag_bandwidth", champsim::bandwidth::maximum_type{1})
                .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))
                .add_parameter("fill_latency", static_cast<uint64_t>(fill_latency))};

  std::array<champsim::operable*, 4> elements{{&uut, &mock_ll, &seed_ul, &mock_ul}};

  for (auto elem : elements) {
    elem->initialize();
    if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
  }

  std::array<champsim::page_number, 12> addresses;
  std::iota(std::begin(addresses), std::end(addresses), champsim::page_number{0xdeadbeef});

  std::vector<decltype(seed_ul)::request_type> packets;
  std::transform(std::begin(addresses), std::end(addresses), std::back_inserter(packets), [](auto addr) {
    // Create a test packet
    decltype(seed_ul)::request_type test;
    test.address = champsim::address{addr};
    test.v_address = test.address;
    test.is_translated = false;
    test.origin = champsim::origin{0, 0};
    return test;
  });

  for (const auto& pkt : packets)
    seed_ul.issue(pkt);

  for (int i = 0; i < 100; ++i) {
    for (auto elem : elements)
      elem->_operate();
  }

  decltype(mock_ul)::request_type test;
  test.address = champsim::address{0xcafebabe};
  test.v_address = champsim::address{0xfeedfeed};
  test.is_translated = true;
  test.origin = champsim::origin{0, 0};
  test.response_requested = true;
  mock_ul.issue(test);

  for (int i = 0; i < 100; ++i) {
    for (auto elem : elements)
      elem->_operate();
  }

  REQUIRE_THAT(mock_ll.addresses, Catch::Matchers::SizeIs(1));
  REQUIRE_THAT(mock_ul.packets, Catch::Matchers::SizeIs(1));
  REQUIRE(mock_ul.packets.back().return_time > 0);
}
