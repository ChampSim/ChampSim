/*
 * Test-only no-op instruction producer. Always EOF, never produces an instruction.
 *
 * Registered as "NULL_INSTRUCTION_PRODUCER" so any test that needs to satisfy a
 * core's required instruction_producer submodule (without driving real
 * instructions) can attach it by model name.
 *
 * Linked only into the test binary; not part of bin/champsim.
 */

#include "instruction.h"
#include "modules.h"
#include "instruction_producer.h"

namespace
{

struct null_instruction_producer_mock : public champsim::modules::instruction_producer {
  explicit null_instruction_producer_mock(champsim::modules::ModuleBuilder /*builder*/) {}

  const ooo_model_instr* peek() override { return nullptr; }
  void consume() override {}
  [[nodiscard]] bool eof() const override { return true; }
};

static champsim::modules::instruction_producer::register_module<null_instruction_producer_mock>
    null_ws_mock_reg("NULL_INSTRUCTION_PRODUCER");

} // anonymous namespace
