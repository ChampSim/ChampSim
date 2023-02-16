#include "catch.hpp"
#include "mocks.hpp"
#include "ooo_cpu.h"

/**
 * The BTB module had a sneaky bug. If:
 *  - There is more than one core, and
 *  - The BTB mispredicts a direct branch, and
 *  - The IP was one less than a multiple of 1024, and
 *  - The IP was not present in the BTB
 * Then, there would be a buffer overflow and a later prediction to some other core will segfault.
 */
TEST_CASE("The basic_btb module does not overflow its bounds.") {
    do_nothing_MRC mock_L1I, mock_L1D;
    O3_CPU uut{0, 1.0, {32, 8, {2}, {2}}, 64, 32, 32, 352, 128, 72, 2, 2, 2, 128, 1, 2, 2, 1, 1, 1, 1, 0, 0, &mock_L1I, 1, &mock_L1D, 1, O3_CPU::bbranchDbimodal, O3_CPU::tbtbDbasic_btb};
    O3_CPU other_cpu{0, 1.0, {32, 8, {2}, {2}}, 64, 32, 32, 352, 128, 72, 2, 2, 2, 128, 1, 2, 2, 1, 1, 1, 1, 0, 0, &mock_L1I, 1, &mock_L1D, 1, O3_CPU::bbranchDbimodal, O3_CPU::tbtbDbasic_btb};

    // Populate the other_cpu's BTB tables
    other_cpu.initialize();
    other_cpu.impl_update_btb(0xffffffff, 0, true, BRANCH_DIRECT_CALL);

    // Access the table from the uut
    uut.initialize();
    (void)uut.impl_btb_prediction(0xdeadbeef);
}

