#include "catch.hpp"
#include "vmem.h"

SCENARIO("Two packets with the same ASID and address translate the same") {
  GIVEN("A large virtual memory") {
    constexpr unsigned vmem_size_bits = 33;
    MEMORY_CONTROLLER dram{1, 3200, 12.5, 12.5, 12.5, 7.5};
    VirtualMemory uut{vmem_size_bits, 1 << 12, 5, 200, dram};

    WHEN("Two packets with the same ASID and address are translated") {
      auto [paddr_a, penalty_a] = uut.va_to_pa(0, 0xdeadbeef);
      auto [paddr_b, penalty_b] = uut.va_to_pa(0, 0xdeadbeef);

      CHECK(penalty_a != penalty_b);

      THEN("Their physical addresses are the same") {
        REQUIRE(paddr_a == paddr_b);
      }
    }
  }
}

SCENARIO("Two packets with a different ASID but same address translate differently") {
  GIVEN("A large virtual memory") {
    constexpr unsigned vmem_size_bits = 33;
    MEMORY_CONTROLLER dram{1, 3200, 12.5, 12.5, 12.5, 7.5};
    VirtualMemory uut{vmem_size_bits, 1 << 12, 5, 200, dram};

    WHEN("Two packets with different ASIDs and same address are translated") {
      auto [paddr_a, penalty_a] = uut.va_to_pa(0, 0xdeadbeef);
      auto [paddr_b, penalty_b] = uut.va_to_pa(1, 0xdeadbeef);

      THEN("Their physical addresses are different") {
        REQUIRE(paddr_a != paddr_b);
      }
    }
  }
}

