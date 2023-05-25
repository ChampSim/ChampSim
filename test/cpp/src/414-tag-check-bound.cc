#include <catch.hpp>
#include "mocks.hpp"
#include "defaults.hpp"

TEST_CASE("Tag checks do not break when translation misses back up") {
  constexpr uint64_t hit_latency = 1;
  constexpr uint64_t fill_latency = 3;
  release_MRC mock_translator;
  do_nothing_MRC mock_ll;
  to_rq_MRP mock_ul{[](auto x, auto y){ return x.v_address == y.v_address; }};
  CACHE uut{CACHE::Builder{champsim::defaults::default_l2c}
    .name("414-uut")
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

  std::array<uint64_t, 12> addresses;
  std::iota(std::begin(addresses), std::end(addresses), 0xdeadbeef);

  std::vector<decltype(mock_ul)::request_type> packets;
  std::transform(std::begin(addresses), std::end(addresses), std::back_inserter(packets), [](auto addr){
      // Create a test packet
      decltype(mock_ul)::request_type test;
      test.address = addr << LOG2_PAGE_SIZE;
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


