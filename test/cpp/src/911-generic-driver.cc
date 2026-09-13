#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <set>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "chrono.h"
#include "modules.h"
#include "operable.h"
#include "phase_controller.h"

// do_cycle is the only orchestration primitive exposed by champsim.cc; the phase loop itself is
// driven by the controllers through the phase_controller interface.
namespace champsim
{
long do_cycle(std::vector<std::reference_wrapper<champsim::operable>>& operables, champsim::chrono::clock& global_clock);
}

namespace
{

// A local driver mirroring champsim::main's loop: each controller owns its phases and informs the
// operables; we tick the operables, advance the controllers, and finish when every one is DONE.
void drive(champsim::modules::environment_module& env,
           std::vector<std::reference_wrapper<champsim::modules::phase_controller>> controllers, champsim::chrono::clock& clock,
           const std::function<void(unsigned)>& on_complete)
{
  using status = champsim::modules::phase_controller::status;
  auto operables = env.typed_view<champsim::operable>("operable");
  std::vector<bool> finished(controllers.size(), false);
  std::set<unsigned> seen;
  std::size_t finished_count = 0;

  for (auto& c : controllers) {
    c.get().advance(0);
  }

  while (finished_count < controllers.size()) {
    clock.tick(champsim::chrono::picoseconds{250});
    auto progress = champsim::do_cycle(operables, clock);
    std::size_t i = 0;
    for (auto& c : controllers) {
      if (finished[i]) {
        ++i;
        continue;
      }
      auto s = c.get().advance(progress);
      for (unsigned idx : c.get().newly_completed_consumers()) {
        if (seen.insert(idx).second && on_complete) {
          on_complete(idx);
        }
      }
      if (s == status::COMPLETE) {
        s = c.get().advance(0); // a phase ended (stats would be collected here); begin the next, or DONE
      }
      if (s == status::DONE) {
        finished[i] = true;
        ++finished_count;
      }
      ++i;
    }
  }
}

void drive_one(champsim::modules::environment_module& env, champsim::modules::phase_controller& controller, champsim::chrono::clock& clock,
               const std::function<void(unsigned)>& on_complete)
{
  drive(env, {std::ref(controller)}, clock, on_complete);
}

// A controller that runs a single ROI phase of the given length (nothing is passed in from outside).
nlohmann::json single_phase(const std::string& name, uint64_t length)
{
  nlohmann::json phases = nlohmann::json::array();
  phases.push_back({{"name", name}, {"is_warmup", false}, {"length", length}});
  return phases;
}

// A trivial operable that counts how many times it was operated
struct counting_operable : public champsim::operable {
  long op_count = 0;
  bool phase_begun = false;
  std::vector<unsigned> ended_phases;

  counting_operable() : champsim::operable(champsim::chrono::picoseconds{250}) {}

  long operate() override
  {
    ++op_count;
    return 1; // always make progress
  }

  void begin_phase(bool) override { phase_begun = true; }
  void end_phase(champsim::stat_report&) override { ended_phases.push_back(0); }
};

// Mock core whose instruction count advances one per operated cycle, so phases complete naturally.
struct mock_core_911 : public champsim::modules::core_module {
  uint64_t instr_count = 0;
  uint8_t cpu_num_ = 0;
  bool producers_eof_ = false; // a live core with attached producers is not at EOF

  explicit mock_core_911(champsim::modules::ModuleBuilder builder) : core_module(champsim::chrono::picoseconds{250})
  {
    cpu_num_ = builder.get_parameter<uint8_t>("cpu_num", true, uint8_t{0});
    set_consumer_id(static_cast<int>(cpu_num_));
  }

  void push_instruction(ooo_model_instr) override {}
  std::size_t instructions_requested() override { return 0; }
  uint64_t sim_instr() const override { return instr_count; }
  uint64_t sim_cycle() const override { return 0; }
  long operate() override
  {
    ++instr_count;
    return 1;
  }
  cpu_stats get_sim_stats() const override { return {}; }
  bool producers_eof() const override { return producers_eof_; }
};

static champsim::modules::core_module::register_module<mock_core_911> mock_core_reg_911("MOCK_CORE_911");

// Mock environment that tracks custom operables AND mock cores
struct mock_env_911 : public champsim::modules::environment_module {
  std::vector<champsim::operable*> operables_;
  std::vector<mock_core_911*> cores_;

  explicit mock_env_911(champsim::modules::ModuleBuilder) {}

  std::vector<std::any> view(const std::string& interface_type) const override
  {
    std::vector<std::any> result;
    if (interface_type == "operable") {
      for (auto* op : operables_)
        result.push_back(op);
      for (auto* c : cores_)
        result.push_back(static_cast<champsim::operable*>(static_cast<champsim::modules::core_module*>(c)));
    } else if (interface_type == "module_lifecycle") {
      for (auto* op : operables_)
        result.push_back(static_cast<champsim::module_lifecycle*>(op));
      for (auto* c : cores_)
        result.push_back(static_cast<champsim::module_lifecycle*>(static_cast<champsim::operable*>(static_cast<champsim::modules::core_module*>(c))));
    } else if (interface_type == "core") {
      for (auto* c : cores_)
        result.push_back(static_cast<champsim::modules::core_module*>(c));
    } else if (interface_type == "packet_consumer") {
      for (auto* c : cores_)
        result.push_back(static_cast<champsim::modules::packet_consumer*>(static_cast<champsim::modules::core_module*>(c)));
    }
    return result;
  }

  const champsim::modules::ModuleBuilder get_builder_params(const std::string&) const override { return champsim::modules::ModuleBuilder(); }
};

static champsim::modules::environment_module::register_module<mock_env_911> mock_env_reg_911("MOCK_ENV_911");

} // anonymous namespace

TEST_CASE("do_cycle operates all operables and returns progress")
{
  auto env_builder = champsim::modules::ModuleBuilder("env_dc", "MOCK_ENV_911");
  auto* env = champsim::modules::environment_module::create_instance(env_builder, static_cast<champsim::modules::environment_module*>(nullptr));
  auto* mock_env = dynamic_cast<mock_env_911*>(env);

  counting_operable op1, op2;
  mock_env->operables_ = {&op1, &op2};

  champsim::chrono::clock clock;
  clock.tick(champsim::chrono::picoseconds{250});

  auto operables = env->typed_view<champsim::operable>("operable");
  auto progress = champsim::do_cycle(operables, clock);

  REQUIRE(progress == 2);
  REQUIRE(op1.op_count == 1);
  REQUIRE(op2.op_count == 1);
}

TEST_CASE("controller completes when the consumer reaches the phase length")
{
  auto env_builder = champsim::modules::ModuleBuilder("env_hook", "MOCK_ENV_911");
  auto* env = champsim::modules::environment_module::create_instance(env_builder, static_cast<champsim::modules::environment_module*>(nullptr));
  auto* mock_env = dynamic_cast<mock_env_911*>(env);

  // The mock core retires one instruction per cycle
  auto core_builder = champsim::modules::ModuleBuilder("core_hook0", "MOCK_CORE_911").add_parameter("cpu_num", uint8_t{0});
  auto* core = champsim::modules::core_module::create_instance(core_builder, env);
  auto* mc = dynamic_cast<mock_core_911*>(core);
  mock_env->cores_ = {mc};

  counting_operable custom_op;
  mock_env->operables_ = {&custom_op};

  auto pc_builder =
      champsim::modules::ModuleBuilder("pc_hook", "PHASE_CONTROLLER").add_parameter("deadlock_cycles", 1000).add_parameter("phases", single_phase("LengthTest", 5));
  auto* controller = champsim::modules::phase_controller::create_instance(pc_builder, env);

  std::vector<unsigned> completed;
  auto on_complete = [&](unsigned idx) { completed.push_back(idx); };

  champsim::chrono::clock clock;
  drive_one(*env, *controller, clock, on_complete);

  REQUIRE(mc->instr_count >= 5);
  REQUIRE(completed.size() == 1);
  REQUIRE(completed[0] == 0);
  REQUIRE(custom_op.phase_begun);
  REQUIRE(custom_op.op_count >= 5);
  REQUIRE(custom_op.ended_phases.size() == 1); // the controller ended the phase on the operables
}

TEST_CASE("controller drive works with a null completion callback")
{
  auto env_builder = champsim::modules::ModuleBuilder("env_null", "MOCK_ENV_911");
  auto* env = champsim::modules::environment_module::create_instance(env_builder, static_cast<champsim::modules::environment_module*>(nullptr));
  auto* mock_env = dynamic_cast<mock_env_911*>(env);

  auto core_builder = champsim::modules::ModuleBuilder("core_null0", "MOCK_CORE_911").add_parameter("cpu_num", uint8_t{0});
  auto* core = champsim::modules::core_module::create_instance(core_builder, env);
  auto* mc = dynamic_cast<mock_core_911*>(core);
  mock_env->cores_ = {mc};

  auto pc_builder =
      champsim::modules::ModuleBuilder("pc_null", "PHASE_CONTROLLER").add_parameter("deadlock_cycles", 1000).add_parameter("phases", single_phase("NullTest", 0));
  auto* controller = champsim::modules::phase_controller::create_instance(pc_builder, env);

  champsim::chrono::clock clock;

  // Should not crash with an empty std::function; length=0 completes on the first advance.
  REQUIRE_NOTHROW(drive_one(*env, *controller, clock, {}));
}

TEST_CASE("multiple controllers each govern their own consumers")
{
  auto env_builder = champsim::modules::ModuleBuilder("env_multi", "MOCK_ENV_911");
  auto* env = champsim::modules::environment_module::create_instance(env_builder, static_cast<champsim::modules::environment_module*>(nullptr));
  auto* mock_env = dynamic_cast<mock_env_911*>(env);

  // Two cores that both retire 1/cycle.
  auto core0_builder = champsim::modules::ModuleBuilder("core_multi0", "MOCK_CORE_911").add_parameter("cpu_num", uint8_t{0});
  auto* mc0 = dynamic_cast<mock_core_911*>(champsim::modules::core_module::create_instance(core0_builder, env));
  auto core1_builder = champsim::modules::ModuleBuilder("core_multi1", "MOCK_CORE_911").add_parameter("cpu_num", uint8_t{1});
  auto* mc1 = dynamic_cast<mock_core_911*>(champsim::modules::core_module::create_instance(core1_builder, env));
  mock_env->cores_ = {mc0, mc1};

  // Controller A governs core 0 only; controller B governs core 1 only (by @-ref to the module).
  auto ml = [](mock_core_911* c) {
    return static_cast<champsim::module_lifecycle*>(static_cast<champsim::operable*>(static_cast<champsim::modules::core_module*>(c)));
  };
  auto pcA_builder = champsim::modules::ModuleBuilder("pc_multiA", "PHASE_CONTROLLER")
                         .add_parameter("deadlock_cycles", 1000)
                         .add_parameter("governs", std::vector<champsim::module_lifecycle*>{ml(mc0)})
                         .add_parameter("phases", single_phase("MultiTest", 7));
  auto* pcA = champsim::modules::phase_controller::create_instance(pcA_builder, env);
  auto pcB_builder = champsim::modules::ModuleBuilder("pc_multiB", "PHASE_CONTROLLER")
                         .add_parameter("deadlock_cycles", 1000)
                         .add_parameter("governs", std::vector<champsim::module_lifecycle*>{ml(mc1)})
                         .add_parameter("phases", single_phase("MultiTest", 7));
  auto* pcB = champsim::modules::phase_controller::create_instance(pcB_builder, env);

  std::vector<unsigned> completed;
  auto on_complete = [&](unsigned idx) { completed.push_back(idx); };

  champsim::chrono::clock clock;
  drive(*env, {std::ref(*pcA), std::ref(*pcB)}, clock, on_complete);

  // Both consumers complete exactly once, and the run ends only when both controllers are DONE.
  REQUIRE(completed.size() == 2);
  REQUIRE(mc0->instr_count >= 7);
  REQUIRE(mc1->instr_count >= 7);
}

TEST_CASE("controller stops when a producer reaches EOF")
{
  auto env_builder = champsim::modules::ModuleBuilder("env_eof2", "MOCK_ENV_911");
  auto* env = champsim::modules::environment_module::create_instance(env_builder, static_cast<champsim::modules::environment_module*>(nullptr));
  auto* mock_env = dynamic_cast<mock_env_911*>(env);

  auto core_builder = champsim::modules::ModuleBuilder("core_eof2_0", "MOCK_CORE_911").add_parameter("cpu_num", uint8_t{0});
  auto* core = champsim::modules::core_module::create_instance(core_builder, env);
  auto* mc = dynamic_cast<mock_core_911*>(core);
  mock_env->cores_ = {mc};

  // The producer is exhausted from the start; the controller observes it through producers_eof().
  mc->producers_eof_ = true;

  auto pc_builder =
      champsim::modules::ModuleBuilder("pc_eof2", "PHASE_CONTROLLER").add_parameter("deadlock_cycles", 1000).add_parameter("phases", single_phase("EOFTest", 999999));
  auto* controller = champsim::modules::phase_controller::create_instance(pc_builder, env);

  std::vector<unsigned> completed;
  auto on_complete = [&](unsigned idx) { completed.push_back(idx); };

  champsim::chrono::clock clock;
  drive_one(*env, *controller, clock, on_complete);

  // Phase should have completed due to EOF
  REQUIRE(completed.size() == 1);
  REQUIRE(completed[0] == 0);
}
