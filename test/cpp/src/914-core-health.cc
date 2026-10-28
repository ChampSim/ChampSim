#include <catch.hpp>

#include "modules.h"

namespace
{

// A minimal core model: the DUT is core_module's health policy, driven
// through the same interface the phase controller uses.
struct mock_core_914 : public champsim::modules::core_module {
  uint64_t instr_count = 0;

  explicit mock_core_914(champsim::modules::ModuleBuilder)
    : core_module(champsim::chrono::picoseconds{250}) {}

  void push_instruction(ooo_model_instr) override {}
  std::size_t instructions_requested() override { return 0; }
  uint64_t sim_instr() const override { return instr_count; }
  uint64_t sim_cycle() const override { return 0; }
  long operate() override { return 0; }
  cpu_stats get_sim_stats() const override { return {}; }
  bool producers_eof() const override { return false; }
};

static champsim::modules::core_module::register_module<mock_core_914> mock_core_reg_914("MOCK_CORE_914");

struct probe_env_914 : champsim::modules::environment_module {
  explicit probe_env_914(champsim::modules::ModuleBuilder) {}
  std::vector<std::any> view(const std::string&) const override { return {}; }
  const champsim::modules::ModuleBuilder get_builder_params(const std::string&) const override { return champsim::modules::ModuleBuilder(); }
};
static champsim::modules::environment_module::register_module<probe_env_914> probe_env_reg_914("PROBE_ENV_914");

mock_core_914* make_core(const std::string& name)
{
  auto env_builder = champsim::modules::ModuleBuilder(name + "_env", "PROBE_ENV_914");
  auto* env = champsim::modules::environment_module::create_instance(env_builder, static_cast<champsim::modules::environment_module*>(nullptr));
  auto core_builder = champsim::modules::ModuleBuilder(name, "MOCK_CORE_914");
  return dynamic_cast<mock_core_914*>(champsim::modules::core_module::create_instance(core_builder, env));
}

} // namespace

TEST_CASE("A core judges its own health from its retirement rate")
{
  using health = champsim::modules::packet_consumer::consumer_health;
  constexpr uint64_t window = 1000000;

  auto* uut = make_core("t914_core");
  uut->reset_health();

  SECTION("A healthy retirement rate reports healthy") {
    uut->instr_count += window; // rate 1.0
    REQUIRE(uut->check_health(window) == health::healthy);
  }

  SECTION("A rate at the warning floor reports warning") {
    uut->instr_count += window / 20; // rate 0.05
    REQUIRE(uut->check_health(window) == health::warning);
  }

  SECTION("A rate at the critical floor reports critical") {
    uut->instr_count += window / 50; // rate 0.02
    REQUIRE(uut->check_health(window) == health::critical);
  }

  SECTION("A stalled consumer reports stalled") {
    uut->instr_count += window / 100; // rate 0.01
    REQUIRE(uut->check_health(window) == health::stalled);
  }

  SECTION("The check window re-baselines: recovery after a bad window is healthy") {
    uut->instr_count += 0; // stalled window
    REQUIRE(uut->check_health(window) == health::stalled);
    uut->instr_count += window; // full-rate window
    REQUIRE(uut->check_health(window) == health::healthy);
  }
}

TEST_CASE("reset_health discards progress made before the phase began")
{
  using health = champsim::modules::packet_consumer::consumer_health;
  constexpr uint64_t window = 1000000;

  auto* uut = make_core("t914_core_reset");
  uut->instr_count = 5 * window; // progress from an earlier phase
  uut->reset_health();

  // Without the reset this window would look extremely healthy; with it,
  // only post-reset progress counts.
  REQUIRE(uut->check_health(window) == health::stalled);
}
