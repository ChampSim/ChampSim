#define CATCH_CONFIG_ENABLE_PAIR_STRINGMAKER
#include <catch.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <algorithm>

#include "../../../btb/basic_btb/basic_btb.h"
#include "instruction.h"

TEST_CASE("The basic_btb does not fill not-taken branches") {
    basic_btb uut;

    std::array<champsim::address, 8> seed_ip{{champsim::address{0x110000}, champsim::address{0x111000}, champsim::address{0x112000}, champsim::address{0x113000}, champsim::address{0x114000}, champsim::address{0x115000}, champsim::address{0x116000}, champsim::address{0x117000}}};
    champsim::address test_ip{0x118000};

    // Fill the BTB ways
    const champsim::address fake_target{0x66b5f0};
    for (auto ip : seed_ip)
      uut.update_btb(ip, fake_target, true, BRANCH_CONDITIONAL);

    // Check that all of the addresses are in the BTB
    std::vector<std::pair<champsim::address, bool>> seed_check_result{};
    std::transform(std::cbegin(seed_ip), std::cend(seed_ip), std::back_inserter(seed_check_result), [&](auto ip){ return uut.btb_prediction(ip); });
    CHECK_THAT(seed_check_result, Catch::Matchers::AllMatch(
          Catch::Matchers::Predicate<std::pair<champsim::address, bool>>(
                 [fake_target](const auto& res){ return res.first == fake_target; },
                 "The predicted target should be "+Catch::to_string(fake_target.to<uint64_t>()))
          ));

    // Attempt to fill with not-taken
    uut.update_btb(test_ip, champsim::address{}, false, BRANCH_CONDITIONAL);

    // The first seeded IP is still present
    auto [seed_predicted_target, seed_always_taken] = uut.btb_prediction(seed_ip.front());
    CHECK_FALSE(seed_predicted_target == champsim::address{}); // Branch target should be known

    // The block is not filled
    auto [predicted_target, always_taken] = uut.btb_prediction(test_ip);
    CHECK(predicted_target == champsim::address{}); // Branch target should not be known
}

