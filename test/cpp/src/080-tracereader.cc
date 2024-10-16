#include <catch.hpp>

#include "tracereader.h"

const std::string trace{{
// Instruction 0
'\x3a', '\x13', '\x00', '\x4c', '\x00', '\x00', '\x00', '\x00', // ip
'\x00', // is branch
'\x00', // branch taken
'\x00', '\x3b', // destination registers
'\x00', '\x00', '\x00', '\x00', // source registers
'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', // dmem0
'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', // dmem1
'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', // smem0
'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', // smem1
'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', // smem2
'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', // smem3

// Instruction 1
'\x3a', '\x16', '\x00', '\x4c', '\x00', '\x00', '\x00', '\x00', // ip
'\x00', // is branch
'\x00', // branch taken
'\x00', '\x49', // destination registers
'\x00', '\x00', '\x00', '\x00', // source registers
'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', // dmem0
'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', // dmem1
'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', // smem0
'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', // smem1
'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', // smem2
'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', // smem3

// Instruction 2
'\x3a', '\x1c', '\x00', '\x4c', '\x00', '\x00', '\x00', '\x00', // ip
'\x00', // is branch
'\x00', // branch taken
'\x00', '\x11', // destination registers
'\x00', '\x06', '\x00', '\x00', // source registers
'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', // dmem0
'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', // dmem1
'\xe8', '\x58', '\x37', '\xb2', '\x7f', '\xfe', '\x00', '\x00', // smem0
'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', // smem1
'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', // smem2
'\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00'  // smem3
}};

TEST_CASE("A tracereader can read the byte representation of an input_instr") {
  champsim::bulk_tracereader<input_instr, std::istringstream> uut{0, std::istringstream{trace}};
  auto inst0 = uut();
  REQUIRE(inst0.ip == champsim::address{0x4c00133a});
  REQUIRE(inst0.is_branch == false);
  REQUIRE_THAT(inst0.destination_registers, Catch::Matchers::RangeEquals(std::vector{59}));
  REQUIRE_THAT(inst0.source_registers, Catch::Matchers::IsEmpty());
  REQUIRE_THAT(inst0.destination_memory, Catch::Matchers::IsEmpty());
  REQUIRE_THAT(inst0.source_memory, Catch::Matchers::IsEmpty());

  auto inst1 = uut();
  REQUIRE(inst1.ip == champsim::address{0x4c00163a});
  REQUIRE(inst1.is_branch == false);
  REQUIRE_THAT(inst1.destination_registers, Catch::Matchers::RangeEquals(std::vector{73}));
  REQUIRE_THAT(inst1.source_registers, Catch::Matchers::IsEmpty());
  REQUIRE_THAT(inst1.destination_memory, Catch::Matchers::IsEmpty());
  REQUIRE_THAT(inst1.source_memory, Catch::Matchers::IsEmpty());
}
