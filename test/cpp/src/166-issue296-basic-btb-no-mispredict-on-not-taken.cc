#include <catch.hpp>
#include "mocks.hpp"
#include "ooo_cpu.h"
#include "defaults.hpp"

TEST_CASE("The basic_btb correctly marks conditional branches as not always taken after they are not taken once") {
    do_nothing_MRC mock_L1I, mock_L1D;
    O3_CPU uut{champsim::defaults::default_core};

    // Access the table from the uut
    uut.initialize();
    uut.impl_update_btb(0x66b60c, 0x66b5f0, true, BRANCH_CONDITIONAL);
    uut.impl_update_btb(0x66b600, 0x66b60c, true, BRANCH_CONDITIONAL);
    uut.impl_update_btb(0x66b60c, 0x66b5f0, false, BRANCH_CONDITIONAL);
    auto [predicted_target, always_taken] = uut.impl_btb_prediction(0x66b60c);
    REQUIRE(always_taken == false); // Branch should not be known to be always taken
}

TEST_CASE("The basic_btb correctly marks conditional branches as not always taken after they are not taken once, even if they are taken later") {
    do_nothing_MRC mock_L1I, mock_L1D;
    O3_CPU uut{champsim::defaults::default_core};

    // Access the table from the uut
    uut.initialize();
    uut.impl_update_btb(0xffffb30a9784, 0xbeefbeef, true, BRANCH_CONDITIONAL);
    uut.impl_update_btb(0xffffb30a9784, 0xbeefbeef, true, BRANCH_CONDITIONAL);
    uut.impl_update_btb(0xffffb30a9784, 0xbeefbeef, true, BRANCH_CONDITIONAL);
    uut.impl_update_btb(0xffffb30a9784, 0xbeefbeef, false, BRANCH_CONDITIONAL);
    uut.impl_update_btb(0xffffb30a9784, 0xbeefbeef, true, BRANCH_CONDITIONAL);
    auto [predicted_target, always_taken] = uut.impl_btb_prediction(0xffffb30a9784);
    REQUIRE(always_taken == false); // Branch should not be known to be always taken
}

