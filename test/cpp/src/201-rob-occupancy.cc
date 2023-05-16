#include <catch.hpp>

#include "reorder_buffer.h"
#include "instr.h"
#include "mocks.hpp"

SCENARIO("The ROB occupancies update on insertion of empty instruction") {
  GIVEN("An empty ROB") {
    constexpr unsigned rob_size = 352;
    constexpr unsigned lq_size = 128;
    constexpr unsigned sq_size = 72;

    do_nothing_MRC mock_L1D;
    champsim::reorder_buffer uut{0, rob_size, lq_size, sq_size, 128, 1, 2, 2, 1, 1, 1, 1, 0, &mock_L1D.queues};

    auto candidate_instr = champsim::test::instruction_with_ip(0xdeadbeef);

    THEN("The ROB would accept an instruction with no memory sources or destinations") {
      REQUIRE(uut.would_accept(candidate_instr));
    }

    WHEN("An instruction with no memory sources or destinations is inserted") {
      uut.push_back(candidate_instr);

      THEN("The occupancy changes, but the size stays the same") {
        CHECK(uut.occupancy() == 1);
        CHECK(uut.size() == rob_size);
        CHECK(uut.lq_occupancy() == 0);
        CHECK(uut.lq_size() == lq_size);
        CHECK(uut.sq_occupancy() == 0);
        CHECK(uut.sq_size() == sq_size);
      }
    }
  }
}

SCENARIO("The ROB occupancies update on insertion of instruction with one memory source") {
  GIVEN("An empty ROB") {
    constexpr unsigned rob_size = 352;
    constexpr unsigned lq_size = 128;
    constexpr unsigned sq_size = 72;

    do_nothing_MRC mock_L1D;
    champsim::reorder_buffer uut{0, rob_size, lq_size, sq_size, 128, 1, 2, 2, 1, 1, 1, 1, 0, &mock_L1D.queues};

    auto candidate_instr = champsim::test::instruction_with_source_memory(0xdeadbeef);

    THEN("The ROB would accept an instruction with one memory source") {
      REQUIRE(uut.would_accept(candidate_instr));
    }

    WHEN("An instruction with one memory source is inserted") {
      uut.push_back(candidate_instr);

      THEN("The occupancy changes, but the size stays the same") {
        CHECK(uut.occupancy() == 1);
        CHECK(uut.size() == rob_size);
        CHECK(uut.lq_occupancy() == 1);
        CHECK(uut.lq_size() == lq_size);
        CHECK(uut.sq_occupancy() == 0);
        CHECK(uut.sq_size() == sq_size);
      }
    }
  }
}

SCENARIO("The ROB occupancies update on insertion of instruction with two memory sources") {
  GIVEN("An empty ROB") {
    constexpr unsigned rob_size = 352;
    constexpr unsigned lq_size = 128;
    constexpr unsigned sq_size = 72;

    do_nothing_MRC mock_L1D;
    champsim::reorder_buffer uut{0, rob_size, lq_size, sq_size, 128, 1, 2, 2, 1, 1, 1, 1, 0, &mock_L1D.queues};

    auto candidate_instr = champsim::test::instruction_with_source_memory(0xdeadbeef, 0xcafebabe);

    THEN("The ROB would accept an instruction with one memory source") {
      REQUIRE(uut.would_accept(candidate_instr));
    }

    WHEN("An instruction with one memory source is inserted") {
      uut.push_back(candidate_instr);

      THEN("The occupancy changes, but the size stays the same") {
        CHECK(uut.occupancy() == 1);
        CHECK(uut.size() == rob_size);
        CHECK(uut.lq_occupancy() == 2);
        CHECK(uut.lq_size() == lq_size);
        CHECK(uut.sq_occupancy() == 0);
        CHECK(uut.sq_size() == sq_size);
      }
    }
  }
}

SCENARIO("The ROB occupancies update on insertion of instruction with one memory destination") {
  GIVEN("An empty ROB") {
    constexpr unsigned rob_size = 352;
    constexpr unsigned lq_size = 128;
    constexpr unsigned sq_size = 72;

    do_nothing_MRC mock_L1D;
    champsim::reorder_buffer uut{0, rob_size, lq_size, sq_size, 128, 1, 2, 2, 1, 1, 1, 1, 0, &mock_L1D.queues};

    auto candidate_instr = champsim::test::instruction_with_destination_memory(0xdeadbeef);

    THEN("The ROB would accept an instruction with one memory destination") {
      REQUIRE(uut.would_accept(candidate_instr));
    }

    WHEN("An instruction with one memory destination is inserted") {
      uut.push_back(candidate_instr);

      THEN("The occupancy changes, but the size stays the same") {
        CHECK(uut.occupancy() == 1);
        CHECK(uut.size() == rob_size);
        CHECK(uut.lq_occupancy() == 0);
        CHECK(uut.lq_size() == lq_size);
        CHECK(uut.sq_occupancy() == 1);
        CHECK(uut.sq_size() == sq_size);
      }
    }
  }
}

SCENARIO("The ROB occupancies update on insertion of instruction with two memory destinations") {
  GIVEN("An empty ROB") {
    constexpr unsigned rob_size = 352;
    constexpr unsigned lq_size = 128;
    constexpr unsigned sq_size = 72;

    do_nothing_MRC mock_L1D;
    champsim::reorder_buffer uut{0, rob_size, lq_size, sq_size, 128, 1, 2, 2, 1, 1, 1, 1, 0, &mock_L1D.queues};

    auto candidate_instr = champsim::test::instruction_with_destination_memory(0xdeadbeef, 0xcafebabe);

    THEN("The ROB would accept an instruction with one memory destination") {
      REQUIRE(uut.would_accept(candidate_instr));
    }

    WHEN("An instruction with one memory destination is inserted") {
      uut.push_back(candidate_instr);

      THEN("The occupancy changes, but the size stays the same") {
        CHECK(uut.occupancy() == 1);
        CHECK(uut.size() == rob_size);
        CHECK(uut.lq_occupancy() == 0);
        CHECK(uut.lq_size() == lq_size);
        CHECK(uut.sq_occupancy() == 2);
        CHECK(uut.sq_size() == sq_size);
      }
    }
  }
}

SCENARIO("The ROB occupancies update on insertion of instruction with one memory source and destination") {
  GIVEN("An empty ROB") {
    constexpr unsigned rob_size = 352;
    constexpr unsigned lq_size = 128;
    constexpr unsigned sq_size = 72;

    do_nothing_MRC mock_L1D;
    champsim::reorder_buffer uut{0, rob_size, lq_size, sq_size, 128, 1, 2, 2, 1, 1, 1, 1, 0, &mock_L1D.queues};

    auto candidate_instr = champsim::test::instruction_with_memory({0xdeadbeef}, {0xcafebabe});

    THEN("The ROB would accept an instruction with one memory destination") {
      REQUIRE(uut.would_accept(candidate_instr));
    }

    WHEN("An instruction with one memory destination is inserted") {
      uut.push_back(candidate_instr);

      THEN("The occupancy changes, but the size stays the same") {
        CHECK(uut.occupancy() == 1);
        CHECK(uut.size() == rob_size);
        CHECK(uut.lq_occupancy() == 1);
        CHECK(uut.lq_size() == lq_size);
        CHECK(uut.sq_occupancy() == 1);
        CHECK(uut.sq_size() == sq_size);
      }
    }
  }
}

SCENARIO("ROB scheduling does not forward an instruction to itself") {
  GIVEN("An empty ROB") {
    constexpr unsigned rob_size = 352;
    constexpr unsigned lq_size = 128;
    constexpr unsigned sq_size = 72;

    do_nothing_MRC mock_L1D;
    champsim::reorder_buffer uut{0, rob_size, lq_size, sq_size, 128, 1, 2, 2, 1, 1, 1, 1, 0, &mock_L1D.queues};

    auto candidate_instr = champsim::test::instruction_with_memory({0xdeadbeef}, {0xdeadbeef});

    THEN("The ROB would accept an instruction with one memory destination") {
      REQUIRE(uut.would_accept(candidate_instr));
    }

    WHEN("An instruction with one memory destination is inserted") {
      uut.push_back(candidate_instr);

      THEN("The occupancy changes, but the size stays the same") {
        CHECK(uut.occupancy() == 1);
        CHECK(uut.size() == rob_size);
        CHECK(uut.lq_occupancy() == 1);
        CHECK(uut.lq_size() == lq_size);
        CHECK(uut.sq_occupancy() == 1);
        CHECK(uut.sq_size() == sq_size);
      }
    }
  }
}
