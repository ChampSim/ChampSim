#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <string>
#include <any>
#include <map>
#include <functional>
#include <nlohmann/json.hpp>

#include "modules.h"
#include "phase_controller.h"
#include "operable.h"

namespace {

// The controller owns its phases; a test supplies them through the builder. begin_phase() then
// steps to the next one and informs the operables, exactly as it does under the real environment.
nlohmann::json one_phase(const std::string& name, bool warmup, uint64_t length)
{
  nlohmann::json phases = nlohmann::json::array();
  phases.push_back({{"name", name}, {"is_warmup", warmup}, {"length", length}});
  return phases;
}

// Mock core with controllable instruction count and EOF state
struct mock_core : public champsim::modules::core_module {
  uint64_t instr_count = 0;
  uint64_t cycle_count = 0;
  uint8_t cpu_num_ = 0;
  bool producers_eof_ = false; // a live core with attached producers is not at EOF

  explicit mock_core(champsim::modules::ModuleBuilder builder)
    : core_module(champsim::chrono::picoseconds{250}) {
    cpu_num_ = builder.get_parameter<uint8_t>("cpu_num", true, uint8_t{0});
    set_consumer_id(static_cast<int>(cpu_num_));
  }

  void push_instruction(ooo_model_instr) override {}
  std::size_t instructions_requested() override { return 0; }
  uint64_t sim_instr() const override { return instr_count; }
  uint64_t sim_cycle() const override { return cycle_count; }
  long operate() override { return 0; }
  cpu_stats get_sim_stats() const override { return {}; }
  bool producers_eof() const override { return producers_eof_; }
  void begin_phase(bool) override {}
};

static champsim::modules::core_module::register_module<mock_core> mock_core_reg_910("MOCK_CORE_910");

// Mock environment that returns controllable mock cores
struct mock_environment : public champsim::modules::environment_module {
  std::vector<mock_core*> cores_;
  int deadlock_cycles_ = 500;

  explicit mock_environment(champsim::modules::ModuleBuilder) {}

  std::vector<std::any> view(const std::string& interface_type) const override {
    std::vector<std::any> result;
    if (interface_type == "core") {
      for (auto* c : cores_) {
        result.push_back(static_cast<champsim::modules::core_module*>(c));
      }
    } else if (interface_type == "operable") {
      for (auto* c : cores_) {
        result.push_back(static_cast<champsim::operable*>(static_cast<champsim::modules::core_module*>(c)));
      }
    } else if (interface_type == "module_lifecycle") {
      for (auto* c : cores_) {
        result.push_back(static_cast<champsim::module_lifecycle*>(static_cast<champsim::operable*>(static_cast<champsim::modules::core_module*>(c))));
      }
    } else if (interface_type == "packet_consumer") {
      for (auto* c : cores_) {
        result.push_back(static_cast<champsim::modules::packet_consumer*>(static_cast<champsim::modules::core_module*>(c)));
      }
    }
    return result;
  }

  int get_deadlock_cycles() const override { return deadlock_cycles_; }

  const champsim::modules::ModuleBuilder get_builder_params(const std::string&) const override {
    return champsim::modules::ModuleBuilder();
  }
};

static champsim::modules::environment_module::register_module<mock_environment> mock_env_reg_910("MOCK_ENV_910");

} // anonymous namespace

TEST_CASE("Phase controller completes when all cores reach instruction count")
{
  auto env_builder = champsim::modules::ModuleBuilder("test_env", "MOCK_ENV_910");
  auto* env = champsim::modules::environment_module::create_instance(env_builder, static_cast<champsim::modules::environment_module*>(nullptr));
  auto* mock_env = dynamic_cast<mock_environment*>(env);

  auto core0_builder = champsim::modules::ModuleBuilder("core0", "MOCK_CORE_910")
    .add_parameter("cpu_num", uint8_t{0});
  auto* core0 = champsim::modules::core_module::create_instance(core0_builder, env);
  auto* mc0 = dynamic_cast<mock_core*>(core0);

  auto core1_builder = champsim::modules::ModuleBuilder("core1", "MOCK_CORE_910")
    .add_parameter("cpu_num", uint8_t{1});
  auto* core1 = champsim::modules::core_module::create_instance(core1_builder, env);
  auto* mc1 = dynamic_cast<mock_core*>(core1);

  mock_env->cores_ = {mc0, mc1};

  auto pc_builder = champsim::modules::ModuleBuilder("pc", "PHASE_CONTROLLER")
    .add_parameter("deadlock_cycles", 1000)
    .add_parameter("phases", one_phase("Test", false, 100));
  auto* pc = champsim::modules::phase_controller::create_instance(pc_builder, env);

  pc->advance(0);

  // Neither core has completed
  auto s = pc->advance(1);
  REQUIRE(s == champsim::modules::phase_controller::status::CONTINUE);
  REQUIRE(pc->newly_completed_consumers().empty());

  // Core 0 reaches target
  mc0->instr_count = 100;
  s = pc->advance(1);
  REQUIRE(s == champsim::modules::phase_controller::status::CONTINUE);
  auto completed = pc->newly_completed_consumers();
  REQUIRE(completed.size() == 1);
  REQUIRE(completed[0] == 0);

  // Core 0 should not re-fire
  s = pc->advance(1);
  REQUIRE(s == champsim::modules::phase_controller::status::CONTINUE);
  REQUIRE(pc->newly_completed_consumers().empty());

  // Core 1 reaches target -> the phase completes; the following advance() reports DONE (no phases left)
  mc1->instr_count = 100;
  s = pc->advance(1);
  REQUIRE(s == champsim::modules::phase_controller::status::COMPLETE);
  completed = pc->newly_completed_consumers();
  REQUIRE(completed.size() == 1);
  REQUIRE(completed[0] == 1);
  REQUIRE(pc->advance(0) == champsim::modules::phase_controller::status::DONE);
}

TEST_CASE("Phase controller detects deadlock on stalled cycles")
{
  auto env_builder = champsim::modules::ModuleBuilder("test_env_dl", "MOCK_ENV_910");
  auto* env = champsim::modules::environment_module::create_instance(env_builder, static_cast<champsim::modules::environment_module*>(nullptr));
  auto* mock_env = dynamic_cast<mock_environment*>(env);

  auto core0_builder = champsim::modules::ModuleBuilder("core_dl0", "MOCK_CORE_910")
    .add_parameter("cpu_num", uint8_t{0});
  auto* core0 = champsim::modules::core_module::create_instance(core0_builder, env);
  auto* mc0 = dynamic_cast<mock_core*>(core0);
  mock_env->cores_ = {mc0};

  // Use a small deadlock cycle limit for testing
  auto pc_builder = champsim::modules::ModuleBuilder("pc_dl", "PHASE_CONTROLLER")
    .add_parameter("deadlock_cycles", 5)
    .add_parameter("phases", one_phase("Deadlock Test", false, 1000));
  auto* pc = champsim::modules::phase_controller::create_instance(pc_builder, env);

  pc->advance(0);

  // Progress > 0: no deadlock
  for (int i = 0; i < 10; ++i) {
    auto s = pc->advance(1);
    REQUIRE(s == champsim::modules::phase_controller::status::CONTINUE);
  }

  // 4 stalled cycles: still CONTINUE
  for (int i = 0; i < 4; ++i) {
    auto s = pc->advance(0);
    REQUIRE(s == champsim::modules::phase_controller::status::CONTINUE);
  }

  // 5th stalled cycle: ABORT
  auto s = pc->advance(0);
  REQUIRE(s == champsim::modules::phase_controller::status::ABORT);
}

TEST_CASE("Phase controller completes all consumers when one producer hits EOF (complete_all)")
{
  auto env_builder = champsim::modules::ModuleBuilder("test_env_eof", "MOCK_ENV_910");
  auto* env = champsim::modules::environment_module::create_instance(env_builder, static_cast<champsim::modules::environment_module*>(nullptr));
  auto* mock_env = dynamic_cast<mock_environment*>(env);

  auto core0_builder = champsim::modules::ModuleBuilder("core_eof0", "MOCK_CORE_910")
    .add_parameter("cpu_num", uint8_t{0});
  auto* core0 = champsim::modules::core_module::create_instance(core0_builder, env);
  auto* mc0 = dynamic_cast<mock_core*>(core0);

  auto core1_builder = champsim::modules::ModuleBuilder("core_eof1", "MOCK_CORE_910")
    .add_parameter("cpu_num", uint8_t{1});
  auto* core1 = champsim::modules::core_module::create_instance(core1_builder, env);
  auto* mc1 = dynamic_cast<mock_core*>(core1);

  mock_env->cores_ = {mc0, mc1};

  auto pc_builder = champsim::modules::ModuleBuilder("pc_eof", "PHASE_CONTROLLER")
    .add_parameter("deadlock_cycles", 1000)
    .add_parameter("phases", one_phase("EOF Test", false, 1000000));
  auto* pc = champsim::modules::phase_controller::create_instance(pc_builder, env);

  pc->advance(0);

  auto s = pc->advance(1);
  REQUIRE(s == champsim::modules::phase_controller::status::CONTINUE);

  // Core 0's producers reach EOF: default policy ends the (only) phase for everyone
  mc0->producers_eof_ = true;
  s = pc->advance(1);
  REQUIRE(s == champsim::modules::phase_controller::status::COMPLETE);
  auto completed = pc->newly_completed_consumers();
  REQUIRE(completed.size() == 2);
  REQUIRE(pc->advance(0) == champsim::modules::phase_controller::status::DONE);
}

TEST_CASE("Phase controller completes only the exhausted consumer under complete_consumer policy")
{
  auto env_builder = champsim::modules::ModuleBuilder("test_env_eofs", "MOCK_ENV_910");
  auto* env = champsim::modules::environment_module::create_instance(env_builder, static_cast<champsim::modules::environment_module*>(nullptr));
  auto* mock_env = dynamic_cast<mock_environment*>(env);

  auto core0_builder = champsim::modules::ModuleBuilder("core_eofs0", "MOCK_CORE_910")
    .add_parameter("cpu_num", uint8_t{0});
  auto* core0 = champsim::modules::core_module::create_instance(core0_builder, env);
  auto* mc0 = dynamic_cast<mock_core*>(core0);

  auto core1_builder = champsim::modules::ModuleBuilder("core_eofs1", "MOCK_CORE_910")
    .add_parameter("cpu_num", uint8_t{1});
  auto* core1 = champsim::modules::core_module::create_instance(core1_builder, env);
  auto* mc1 = dynamic_cast<mock_core*>(core1);

  mock_env->cores_ = {mc0, mc1};

  auto pc_builder = champsim::modules::ModuleBuilder("pc_eofs", "PHASE_CONTROLLER")
    .add_parameter("deadlock_cycles", 1000)
    .add_parameter("eof_policy", std::string{"complete_consumer"})
    .add_parameter("phases", one_phase("EOF Producer Test", false, 100));
  auto* pc = champsim::modules::phase_controller::create_instance(pc_builder, env);

  pc->advance(0);

  // Core 0's producers reach EOF: only consumer 0 completes
  mc0->producers_eof_ = true;
  auto s = pc->advance(1);
  REQUIRE(s == champsim::modules::phase_controller::status::CONTINUE);
  auto completed = pc->newly_completed_consumers();
  REQUIRE(completed.size() == 1);
  REQUIRE(completed[0] == 0);

  // Core 1 finishes by progress: the (only) phase completes
  mc1->instr_count = 100;
  s = pc->advance(1);
  REQUIRE(s == champsim::modules::phase_controller::status::COMPLETE);
  completed = pc->newly_completed_consumers();
  REQUIRE(completed.size() == 1);
  REQUIRE(completed[0] == 1);
  REQUIRE(pc->advance(0) == champsim::modules::phase_controller::status::DONE);
}

TEST_CASE("Phase controller resets state between phases")
{
  auto env_builder = champsim::modules::ModuleBuilder("test_env_reset", "MOCK_ENV_910");
  auto* env = champsim::modules::environment_module::create_instance(env_builder, static_cast<champsim::modules::environment_module*>(nullptr));
  auto* mock_env = dynamic_cast<mock_environment*>(env);

  auto core0_builder = champsim::modules::ModuleBuilder("core_reset0", "MOCK_CORE_910")
    .add_parameter("cpu_num", uint8_t{0});
  auto* core0 = champsim::modules::core_module::create_instance(core0_builder, env);
  auto* mc0 = dynamic_cast<mock_core*>(core0);
  mock_env->cores_ = {mc0};

  // Two phases owned by the controller: Warmup(100) then Simulation(200).
  nlohmann::json phases = nlohmann::json::array();
  phases.push_back({{"name", "Phase1"}, {"is_warmup", true}, {"length", 100}});
  phases.push_back({{"name", "Phase2"}, {"is_warmup", false}, {"length", 200}});

  auto pc_builder = champsim::modules::ModuleBuilder("pc_reset", "PHASE_CONTROLLER")
    .add_parameter("deadlock_cycles", 1000)
    .add_parameter("phases", phases);
  auto* pc = champsim::modules::phase_controller::create_instance(pc_builder, env);

  // Phase 1: complete immediately — progress reaches the phase length on first advance
  mc0->instr_count = 0;
  pc->advance(0);
  REQUIRE(pc->phase().name == "Phase1");
  mc0->instr_count = 100;  // delta = 100 - baseline(0) = 100 >= length(100) → phase 1 done, more remain
  auto s = pc->advance(1);
  REQUIRE(s == champsim::modules::phase_controller::status::COMPLETE);

  // Phase 2: baseline = 100 (instr_count at Phase 2 start), length = 200
  pc->advance(0);
  REQUIRE(pc->phase().name == "Phase2");
  s = pc->advance(1);
  REQUIRE(s == champsim::modules::phase_controller::status::CONTINUE);

  mc0->instr_count = 300;  // delta = 300 - baseline(100) = 200 >= length(200) → last phase completes
  s = pc->advance(1);
  REQUIRE(s == champsim::modules::phase_controller::status::COMPLETE);
  REQUIRE(pc->advance(0) == champsim::modules::phase_controller::status::DONE);
}
