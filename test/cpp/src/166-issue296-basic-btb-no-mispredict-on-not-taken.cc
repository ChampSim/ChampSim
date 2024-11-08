#include <catch.hpp>

#include "../../../btb/basic_btb/basic_btb.h"
#include "instruction.h"

TEST_CASE("The basic_btb correctly marks conditional branches as not always taken after they are not taken once") {
    basic_btb uut;

    // Access the table from the uut
    uut.update_btb(champsim::address{0x66b60c}, champsim::address{0x66b5f0}, true, BRANCH_CONDITIONAL);
    uut.update_btb(champsim::address{0x66b600}, champsim::address{0x66b60c}, true, BRANCH_CONDITIONAL);
    uut.update_btb(champsim::address{0x66b60c}, champsim::address{0x66b5f0}, false, BRANCH_CONDITIONAL);
    auto [predicted_target, always_taken] = uut.btb_prediction(champsim::address{0x66b60c});
    REQUIRE(always_taken == false); // Branch should not be known to be always taken
}

TEST_CASE("The basic_btb correctly marks conditional branches as not always taken after they are not taken once, even if they are taken later") {
    basic_btb uut;

    // Access the table from the uut
    uut.update_btb(champsim::address{0xffffb30a9784}, champsim::address{0xbeefbeef}, true, BRANCH_CONDITIONAL);
    uut.update_btb(champsim::address{0xffffb30a9784}, champsim::address{0xbeefbeef}, true, BRANCH_CONDITIONAL);
    uut.update_btb(champsim::address{0xffffb30a9784}, champsim::address{0xbeefbeef}, true, BRANCH_CONDITIONAL);
    uut.update_btb(champsim::address{0xffffb30a9784}, champsim::address{0xbeefbeef}, false, BRANCH_CONDITIONAL);
    uut.update_btb(champsim::address{0xffffb30a9784}, champsim::address{0xbeefbeef}, true, BRANCH_CONDITIONAL);
    auto [predicted_target, always_taken] = uut.btb_prediction(champsim::address{0xffffb30a9784});
    REQUIRE(always_taken == false); // Branch should not be known to be always taken
}

