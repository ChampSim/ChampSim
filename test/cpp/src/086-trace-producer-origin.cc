#include <catch.hpp>
#include <cstdio>
#include <fstream>
#include <string>

#include "modules.h"
#include "instruction_producer.h"
#include "origin.h"

namespace
{

// The consumer whose identity the producer should inherit.
struct probe_consumer : champsim::modules::packet_consumer {
  explicit probe_consumer(int id) { set_consumer_id(id); }
};

// One valid 64-byte input_instr record (same bytes as 080-tracereader).
const std::string one_instr{{
    '\x3a', '\x13', '\x00', '\x4c', '\x00', '\x00', '\x00', '\x00', // ip
    '\x00', '\x00',                                                 // is branch, taken
    '\x00', '\x3b',                                                 // destination registers
    '\x00', '\x00', '\x00', '\x00',                                 // producer registers
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', // dmem0
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', // dmem1
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', // smem0
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', // smem1
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', // smem2
    '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00', '\x00'  // smem3
}};

// Write a small plain (uncompressed) trace file and return its path.
std::string write_trace(const std::string& tag)
{
  auto path = std::string{"/tmp/t086_"} + tag + ".champsimtrace";
  std::ofstream out{path, std::ios::binary};
  for (int i = 0; i < 4; ++i) {
    out.write(one_instr.data(), static_cast<std::streamsize>(one_instr.size()));
  }
  return path;
}

} // namespace

TEST_CASE("A trace producer stamps packets with its consumer's id, producer id defaulting to it")
{
  auto path = write_trace("default");
  probe_consumer consumer{3};

  auto builder = champsim::modules::ModuleBuilder{"t086_src_default", "INSTRUCTION_PRODUCER"}
    .add_parameter("trace_file", path);
  auto* uut = champsim::modules::instruction_producer::create_instance(builder, &consumer);
  auto* typed = dynamic_cast<champsim::modules::instruction_producer*>(uut);
  REQUIRE(typed != nullptr);

  const auto* instr = typed->peek();
  REQUIRE(instr != nullptr);
  // Consumer identity comes from the bound consumer; the producer id inherits it
  REQUIRE(instr->origin.consumer() == 3);
  REQUIRE(instr->origin.producer() == 3);
  REQUIRE(instr->origin.cpu() == 3);
  REQUIRE(instr->origin.asid() == 3);

  std::remove(path.c_str());
}

TEST_CASE("A framework-assigned producer id overrides the default")
{
  auto path = write_trace("override");
  probe_consumer consumer{3};

  auto builder = champsim::modules::ModuleBuilder{"t086_src_override", "INSTRUCTION_PRODUCER"}
    .add_parameter("trace_file", path);
  auto* uut = champsim::modules::instruction_producer::create_instance(builder, &consumer);
  uut->set_producer_id(7); // as the startup identity pass would
  auto* typed = dynamic_cast<champsim::modules::instruction_producer*>(uut);
  REQUIRE(typed != nullptr);

  const auto* instr = typed->peek();
  REQUIRE(instr != nullptr);
  // Two coordinates, independently owned: hardware context vs producer id
  REQUIRE(instr->origin.consumer() == 3);
  REQUIRE(instr->origin.producer() == 7);

  std::remove(path.c_str());
}

TEST_CASE("A trace producer describes itself with its trace path")
{
  auto path = write_trace("describe");
  probe_consumer consumer{0};

  auto builder = champsim::modules::ModuleBuilder{"t086_src_describe", "INSTRUCTION_PRODUCER"}
    .add_parameter("trace_file", path);
  auto* uut = champsim::modules::instruction_producer::create_instance(builder, &consumer);

  REQUIRE(uut->describe() == path);

  std::remove(path.c_str());
}
