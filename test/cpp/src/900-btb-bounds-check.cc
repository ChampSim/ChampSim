#include <catch.hpp>
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
    O3_CPU uut{champsim::core_builder{}};
    O3_CPU other_cpu{champsim::core_builder{}};

    // Populate the other_cpu's BTB tables
    other_cpu.initialize();
    other_cpu.impl_update_btb(champsim::address{0xffffffff}, champsim::address{}, true, BRANCH_DIRECT_CALL);

    // Access the table from the uut
    uut.initialize();
    (void)uut.impl_btb_prediction(champsim::address{0xdeadbeef}, 0);

    SUCCEED();
}

