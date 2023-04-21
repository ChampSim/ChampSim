#define CATCH_CONFIG_ENABLE_PAIR_STRINGMAKER
#include <catch.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <algorithm>

#include "mocks.hpp"
#include "ooo_cpu.h"
#include "defaults.hpp"

TEST_CASE("The basic_btb does not fill not-taken branches") {
    do_nothing_MRC mock_L1I, mock_L1D;
    O3_CPU uut{champsim::defaults::default_core};

    std::array<uint64_t, 8> seed_ip{{0x110000, 0x111000, 0x112000, 0x113000, 0x114000, 0x115000, 0x116000, 0x117000}};
    uint64_t test_ip = 0x118000;

    uut.initialize();

    // Fill the BTB ways
    const uint64_t fake_target = 0x66b5f0;
    for (auto ip : seed_ip)
      uut.impl_update_btb(ip, fake_target, true, BRANCH_CONDITIONAL);

    // Check that all of the addresses are in the BTB
    std::vector<std::pair<uint64_t, uint8_t>> seed_check_result{};
    std::transform(std::cbegin(seed_ip), std::cend(seed_ip), std::back_inserter(seed_check_result), [&](auto ip){ return uut.impl_btb_prediction(ip); });
    CHECK_THAT(seed_check_result, Catch::Matchers::AllMatch(
          Catch::Matchers::Predicate<std::pair<uint64_t, uint8_t>>(
                 [fake_target](const auto& res){ return res.first == fake_target; },
                 "The predicted target should be "+Catch::to_string(fake_target))
          ));

    // Attempt to fill with not-taken
    uut.impl_update_btb(test_ip, 0, false, BRANCH_CONDITIONAL);

    // The first seeded IP is still present
    auto [seed_predicted_target, seed_always_taken] = uut.impl_btb_prediction(seed_ip.front());
    CHECK_FALSE(seed_predicted_target == 0); // Branch target should be known

    // The block is not filled
    auto [predicted_target, always_taken] = uut.impl_btb_prediction(test_ip);
    CHECK(predicted_target == 0); // Branch target should not be known
}

