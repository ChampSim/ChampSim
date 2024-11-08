#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"

TEST_CASE("Tag checks do not break when translation misses back up") {
  constexpr uint64_t hit_latency = 1;
  constexpr uint64_t fill_latency = 3;
  release_MRC mock_translator;
  do_nothing_MRC mock_ll;
  to_rq_MRP mock_ul{[](auto x, auto y){ return x.v_address == y.v_address; }};
  CACHE uut{champsim::cache_builder{champsim::defaults::default_l2c}
    .name("414a-uut")
      .upper_levels({&mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .lower_translate(&mock_translator.queues)
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
  };

  std::array<champsim::operable*, 4> elements{{&uut, &mock_ll, &mock_ul, &mock_translator}};

  for (auto elem : elements) {
    elem->initialize();
    elem->warmup = false;
    elem->begin_phase();
  }

  std::array<champsim::page_number, 12> addresses;
  std::iota(std::begin(addresses), std::end(addresses), champsim::page_number{0xdeadb});

  std::vector<decltype(mock_ul)::request_type> packets;
  std::transform(std::begin(addresses), std::end(addresses), std::back_inserter(packets), [](auto addr){
      // Create a test packet
      decltype(mock_ul)::request_type test;
      test.address = champsim::address{addr};
      test.v_address = test.address;
      test.is_translated = false;
      test.cpu = 0;
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

TEST_CASE("Backed up translation misses do not prevent translated packets from advancing") {
  constexpr uint64_t hit_latency = 1;
  constexpr uint64_t fill_latency = 3;
  champsim::channel refusal_channel{0,0,0,champsim::data::bits{},false};
  do_nothing_MRC mock_ll;
  to_rq_MRP seed_ul{[](auto x, auto y){ return x.v_address == y.v_address; }};
  to_rq_MRP mock_ul{[](auto x, auto y){ return x.v_address == y.v_address; }};
  CACHE uut{champsim::cache_builder{champsim::defaults::default_l2c}
    .name("414b-uut")
      .upper_levels({&seed_ul.queues, &mock_ul.queues})
      .lower_level(&mock_ll.queues)
      .lower_translate(&refusal_channel)
      .mshr_size(1)
      .tag_bandwidth(champsim::bandwidth::maximum_type{1})
      .hit_latency(hit_latency)
      .fill_latency(fill_latency)
  };

  std::array<champsim::operable*, 4> elements{{&uut, &mock_ll, &seed_ul, &mock_ul}};

  for (auto elem : elements) {
    elem->initialize();
    elem->warmup = false;
    elem->begin_phase();
  }

  std::array<champsim::page_number, 12> addresses;
  std::iota(std::begin(addresses), std::end(addresses), champsim::page_number{0xdeadbeef});

  std::vector<decltype(seed_ul)::request_type> packets;
  std::transform(std::begin(addresses), std::end(addresses), std::back_inserter(packets), [](auto addr){
      // Create a test packet
      decltype(seed_ul)::request_type test;
      test.address = champsim::address{addr};
      test.v_address = test.address;
      test.is_translated = false;
      test.cpu = 0;
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
  test.cpu = 0;
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

