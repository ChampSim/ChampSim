#include <catch.hpp>
#include "mocks.hpp"
#include "ooo_cpu.h"

TEST_CASE("The basic_btb correctly marks conditional branches as not always taken after they are not taken once") {
    do_nothing_MRC mock_L1I, mock_L1D;
    O3_CPU uut{0, 1.0, {32, 8, {2}, {2}}, 64, 32, 32, 352, 128, 72, 2, 2, 2, 128, 1, 2, 2, 1, 1, 1, 1, 0, 0, &mock_L1I, 1, &mock_L1D, 1, O3_CPU::bbranchDbimodal, O3_CPU::tbtbDbasic_btb};

    // Access the table from the uut
    uut.initialize();
    uut.impl_update_btb(champsim::address{0x66b60c}, champsim::address{0x66b5f0}, true, BRANCH_CONDITIONAL);
    uut.impl_update_btb(champsim::address{0x66b600}, champsim::address{0x66b60c}, true, BRANCH_CONDITIONAL);
    uut.impl_update_btb(champsim::address{0x66b60c}, champsim::address{0x66b5f0}, false, BRANCH_CONDITIONAL);
    auto [predicted_target, always_taken] = uut.impl_btb_prediction(champsim::address{0x66b60c});
    REQUIRE(always_taken == false); // Branch should not be known to be always taken
}

TEST_CASE("The basic_btb correctly marks conditional branches as not always taken after they are not taken once, even if they are taken later") {
    do_nothing_MRC mock_L1I, mock_L1D;
    O3_CPU uut{0, 1.0, {32, 8, {2}, {2}}, 64, 32, 32, 352, 128, 72, 2, 2, 2, 128, 1, 2, 2, 1, 1, 1, 1, 0, 0, &mock_L1I, 1, &mock_L1D, 1, O3_CPU::bbranchDbimodal, O3_CPU::tbtbDbasic_btb};

    // Access the table from the uut
    uut.initialize();
    uut.impl_update_btb(champsim::address{0xffffb30a9784}, champsim::address{0xbeefbeef}, true, BRANCH_CONDITIONAL);
    uut.impl_update_btb(champsim::address{0xffffb30a9784}, champsim::address{0xbeefbeef}, true, BRANCH_CONDITIONAL);
    uut.impl_update_btb(champsim::address{0xffffb30a9784}, champsim::address{0xbeefbeef}, true, BRANCH_CONDITIONAL);
    uut.impl_update_btb(champsim::address{0xffffb30a9784}, champsim::address{0xbeefbeef}, false, BRANCH_CONDITIONAL);
    uut.impl_update_btb(champsim::address{0xffffb30a9784}, champsim::address{0xbeefbeef}, true, BRANCH_CONDITIONAL);
    auto [predicted_target, always_taken] = uut.impl_btb_prediction(champsim::address{0xffffb30a9784});
    REQUIRE(always_taken == false); // Branch should not be known to be always taken
}

