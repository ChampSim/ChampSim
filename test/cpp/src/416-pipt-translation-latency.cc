#include <catch.hpp>

#include "channel.h"
#include "defaults.hpp"
#include "mocks.hpp"

// PIPT translation-timing fix: for a translating cache (lower_translate !=
// nullptr), an UNTRANSLATED access cannot tag-check until translation yields the
// physical set index, so translation latency is ADDITIVE, never hidden under
// HIT_LATENCY. The bug only surfaced for FAST (TLB-hit) translations that fit
// inside the HIT_LATENCY window (fully overlapped); this test uses
// translate_latency < hit_latency to pin that case. 412 pins the slow case
// (translator latency > HIT_LATENCY), which was additive even pre-fix.
TEMPLATE_TEST_CASE("An untranslated access serializes translation before its tag check (PIPT)", "", to_wq_MRP, to_rq_MRP, to_pq_MRP)
{
  GIVEN("An empty cache with a translator faster than its hit latency")
  {
    constexpr uint64_t hit_latency = 10;
    constexpr uint64_t fill_latency = 3;
    constexpr uint64_t translate_latency = 4; // strictly < hit_latency: the overlapped (TLB-hit) case the bug hid
    do_nothing_MRC mock_translator{static_cast<int>(translate_latency)};
    do_nothing_MRC mock_ll;
    TestType mock_ul{[](auto x, auto y) {
      return x.v_address == y.v_address;
    }};
    CACHE uut{champsim::modules::ModuleBuilder{"t416_cache", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
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

    WHEN("An untranslated packet is sent")
    {
      typename TestType::request_type test;
      test.address = champsim::address{0xdeadbeef};
      test.v_address = champsim::address{0xdeadbeef};
      test.is_translated = false;
      test.origin = champsim::origin{0, 0};

      auto test_result = mock_ul.issue(test);
      THEN("The issue is accepted") { REQUIRE(test_result); }

      for (int i = 0; i < 100; ++i) {
        for (auto elem : elements)
          elem->_operate();
      }

      THEN("The packet is translated and forwarded on a miss")
      {
        REQUIRE(std::size(mock_ll.addresses) == 1);
        REQUIRE(mock_ll.addresses.front() == champsim::address{0x11111eef});
      }

      THEN("Translation latency is additive, not hidden under the hit latency")
      {
        // Additive total: translate + hit + fill + 2 (the +2 is fixed
        // inter-element clocking, as in 412). Since translate(4) < hit(10), the
        // buggy overlapped model would hide translation and return
        // hit+fill+2==15; the additive value is 19.
        constexpr uint64_t additive = translate_latency + hit_latency + fill_latency + 2;
        constexpr uint64_t overlapped_buggy = hit_latency + fill_latency + 2;
        static_assert(additive > overlapped_buggy, "the fix must make the fast-translation path strictly slower than the overlapped bug");
        REQUIRE_THAT(mock_ul.packets.front(), champsim::test::ReturnedMatcher(additive, 1));
      }
    }
  }
}

// A pre-translated access into the same translating cache never translates, so
// its tag check is timed from admission + HIT_LATENCY as before. The delta from
// the untranslated case above is precisely the translation round trip.
TEST_CASE("A pre-translated access into a translating cache is not delayed by translation")
{
  constexpr uint64_t hit_latency = 10;
  constexpr uint64_t fill_latency = 3;
  constexpr uint64_t translate_latency = 4;
  do_nothing_MRC mock_translator{static_cast<int>(translate_latency)};
  do_nothing_MRC mock_ll;
  to_rq_MRP mock_ul{[](auto x, auto y) {
    return x.v_address == y.v_address;
  }};
  CACHE uut{champsim::modules::ModuleBuilder{"t416_cache_pretranslated", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
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

  WHEN("A pre-translated packet is sent")
  {
    to_rq_MRP::request_type test;
    test.address = champsim::address{0xdeadbeef};
    test.v_address = champsim::address{0xdeadbeef};
    test.is_translated = true;
    test.origin = champsim::origin{0, 0};

    REQUIRE(mock_ul.issue(test));

    for (int i = 0; i < 100; ++i) {
      for (auto elem : elements)
        elem->_operate();
    }

    THEN("No translation was issued") { REQUIRE(mock_translator.packet_count() == 0); }

    THEN("It returns in hit_latency + fill_latency + 2 (translation not on its path)")
    {
      // Same clocking overhead as the untranslated case above, minus the
      // translation round trip: exactly translate_latency cycles faster.
      constexpr uint64_t pre_translated = hit_latency + fill_latency + 2;
      REQUIRE_THAT(mock_ul.packets.front(), champsim::test::ReturnedMatcher(pre_translated, 1));
    }
  }
}

// A cache with no translator (lower_translate == nullptr) is a TLB / any
// non-translating cache. The fixed path is gated on (lower_translate != nullptr
// && !is_translated), so it is never entered: every access tag-checks from
// admission + HIT_LATENCY, byte-identical to pre-fix. 401 corroborates.
SCENARIO("A non-translating (TLB-like) cache tag-checks without waiting on translation")
{
  GIVEN("An empty cache with no translator")
  {
    constexpr uint64_t hit_latency = 7;
    do_nothing_MRC mock_ll;
    to_rq_MRP mock_ul;
    CACHE uut{champsim::modules::ModuleBuilder{"t416_tlb_like", "DEFAULT_CACHE", champsim::defaults::default_l1d()}
                  .add_parameter("mshr_size", static_cast<uint32_t>(8))
                  .add_parameter("upper_levels", std::vector<champsim::modules::channel_module*>{&mock_ul.queues})
                  .add_parameter("lower_level", static_cast<champsim::modules::channel_module*>(&mock_ll.queues))
                  .add_parameter("hit_latency", static_cast<uint64_t>(hit_latency))};

    // lower_translate defaults to nullptr in default_l1d(): this is the TLB case.
    REQUIRE(uut.lower_translate == nullptr);

    std::array<champsim::operable*, 3> elements{{&uut, &mock_ll, &mock_ul}};
    for (auto elem : elements) {
      elem->initialize();
      if (auto* mp = dynamic_cast<champsim::module_lifecycle*>(elem)) { mp->begin_phase(false); };
    }

    WHEN("A pre-translated line is filled and then hit")
    {
      to_rq_MRP::request_type seed;
      seed.address = champsim::address{0xdeadbeef};
      seed.is_translated = true; // a TLB receives is_translated == true requests (issue_translation sets it)
      seed.instr_id = 1;
      seed.origin = champsim::origin{0, 0};
      REQUIRE(mock_ul.issue(seed));

      for (int i = 0; i < 100; ++i)
        for (auto elem : elements)
          elem->_operate();

      auto test = seed;
      test.instr_id = 2;
      REQUIRE(mock_ul.issue(test));

      for (uint64_t i = 0; i < 2 * hit_latency; ++i)
        for (auto elem : elements)
          elem->_operate();

      THEN("The hit returns in exactly HIT_LATENCY, timed from admission (byte-identical to pre-fix)")
      {
        REQUIRE_THAT(mock_ul.packets, Catch::Matchers::SizeIs(2));
        REQUIRE_THAT(mock_ul.packets.back(), champsim::test::ReturnedMatcher(hit_latency, 1));
      }
    }
  }
}
